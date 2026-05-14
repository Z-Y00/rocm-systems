# Copyright (c) Advanced Micro Devices, Inc.
# SPDX-License-Identifier:  MIT
# ruff: noqa

"""Instrument PyTorch operators with ROCTX ranges so rocprofiler-compute
can map GPU kernels back to operator names.

Uses TorchDispatchMode for dispatched ATen ops, plus explicit Python
wrappers around nn.Module.__call__, Optimizer.step, Tensor.backward,
torch.cuda.set_device, and torch.distributed collectives.

Usage: python inject_roctx.py main.py --epochs 1 --batch-size 4
"""

import os
import sys
from pathlib import Path

# Extend sys.path so the sibling 'utils' package (logger, etc.) is
# importable when this script is launched directly by rocprofv3.
script_dir = Path(__file__).resolve().parent
sys.path.insert(0, str(script_dir.parent))

# Locate the roctx Python bindings shipped with rocprofiler-sdk by
# adding the per-Python-version directories under ROCM_PATH to sys.path.
rocm_root = os.environ.get("ROCM_PATH", "/opt/rocm")
python_version = f"python{sys.version_info.major}.{sys.version_info.minor}"
candidate_paths = [
    f"{rocm_root}/lib/{python_version}/site-packages",
    f"{rocm_root}/libexec/rocprofiler-sdk/python",
]

for candidate in candidate_paths:
    if candidate not in sys.path:
        sys.path.insert(0, candidate)

from utils.logger import console_error, console_log, console_warning

console_log("torch trace", f"Workload Python Version: {python_version}")

try:
    from roctx import rangePop, rangePush

    if hasattr(rangePush, "__code__") and hasattr(rangePush.__code__, "co_filename"):
        roctx_path = Path(rangePush.__code__.co_filename).parent
    else:
        roctx_path = "<unknown>"

    console_log(
        "torch trace",
        f"ROCTX module loaded from: {roctx_path}",
    )
except ImportError:
    console_error(
        f"Looked for roctx in: {candidate_paths}\n"
        "ROCTX not found. --torch-trace requires roctx from rocprofiler-sdk. "
        "Ensure your workload uses a Python version for which "
        "roctx bindings are available in your ROCm installation.\n",
    )
    sys.exit(1)

try:
    import torch
    import torch._C

    console_log(f"PyTorch version: {torch.__version__}")
except ImportError:
    console_warning(
        "PyTorch is not installed or not properly configured.\n"
        "The --torch-trace option requires a valid PyTorch installation.\n"
        "Please install PyTorch and try again."
    )
    sys.exit(0)

import importlib.util
import inspect
import threading
from functools import wraps

# Per-thread marker_stack and context_stack. Both stacks are pushed
# and popped together to assemble 'op/op/op:#N@file:line/...' markers.
# utils_analysis.py splits the final string on the literal ':#'.
_thread_local = threading.local()

# Module-level reference to the active TorchDispatchMode, kept so the
# mode object outlives the function that installed it. The dispatcher
# stays active for the lifetime of the process; we never call __exit__.
_active_dispatch_mode = None


def get_marker_stack():
    if not hasattr(_thread_local, "marker_stack"):
        _thread_local.marker_stack = []
    return _thread_local.marker_stack


def get_context_stack():
    if not hasattr(_thread_local, "context_stack"):
        _thread_local.context_stack = []
    return _thread_local.context_stack


def resolve_user_caller_location():
    """Return 'file:line' for the nearest frame outside this module and
    the torch installation directory, or 'dispatcher:0' if no such
    frame is present on the stack."""
    try:
        torch_root = str(Path(torch.__file__).resolve().parent) + os.sep
    except Exception:
        torch_root = ""
    this_file = __file__
    frame = inspect.currentframe()
    while frame is not None:
        fn_path = frame.f_code.co_filename
        if fn_path != this_file and torch_root and not fn_path.startswith(torch_root):
            return f"{Path(fn_path).name}:{frame.f_lineno}"
        frame = frame.f_back
    return "dispatcher:0"


# ---------------------------------------------------------------------------
# Structural wrappers installed at module load (dist.* collectives and
# torch.cuda.set_device). These entry points bypass the ATen dispatcher
# and are therefore not intercepted by TorchDispatchMode.
# ---------------------------------------------------------------------------


def roctx_wrapper(func, name=None):
    """Wrap func with a ROCTX range, pushing onto the per-thread marker
    stacks so nested dispatcher markers inherit the hierarchical prefix."""
    func_name = name or func.__name__
    call_counter = {"count": 0}

    @wraps(func)
    def wrapper(*args, **kwargs):
        call_counter["count"] += 1
        location = resolve_user_caller_location()

        marker_stack = get_marker_stack()
        context_stack = get_context_stack()
        marker_stack.append(func_name)
        context_stack.append(f"#{call_counter['count']}@{location}")
        full_marker_name = "/".join(marker_stack) + ":" + "/".join(context_stack)

        rangePush(full_marker_name)
        try:
            result = func(*args, **kwargs)
        finally:
            rangePop()
            marker_stack.pop()
            context_stack.pop()
        return result

    return wrapper


# Collectives wrapped at module load. Any collective omitted from this
# list will have its RCCL/NCCL kernels captured by rocprofiler without
# an enclosing operator marker.
DISTRIBUTED_COLLECTIVE_NAMES = (
    "all_reduce",
    "all_gather",
    "all_gather_into_tensor",
    "all_gather_object",
    "reduce_scatter",
    "reduce_scatter_tensor",
    "broadcast",
    "broadcast_object_list",
    "reduce",
    "gather",
    "gather_object",
    "scatter",
    "scatter_object_list",
    "all_to_all",
    "all_to_all_single",
    "send",
    "recv",
    "isend",
    "irecv",
    "barrier",
    "monitored_barrier",
)


def patch_distributed_collectives():
    """Wrap each entry in DISTRIBUTED_COLLECTIVE_NAMES, and every
    function defined in torch.distributed._functional_collectives.
    Entries not present on the current PyTorch build are skipped."""
    try:
        import torch.distributed as dist
    except Exception as e:
        console_warning(
            "torch trace",
            f"torch.distributed not importable; collectives will not be marked: {e}",
        )
        return

    wrapped = []
    for fn_name in DISTRIBUTED_COLLECTIVE_NAMES:
        fn = getattr(dist, fn_name, None)
        if fn is None or not callable(fn):
            continue
        try:
            setattr(dist, fn_name, roctx_wrapper(fn, f"torch.distributed.{fn_name}"))
            wrapped.append(fn_name)
        except Exception as e:
            console_warning(
                "torch trace",
                f"Could not patch torch.distributed.{fn_name}: {e}",
            )

    # torch.distributed._functional_collectives is used by FSDP2 and
    # DTensor; it is not present on every PyTorch build.
    try:
        import torch.distributed._functional_collectives as fc

        for fn_name in dir(fc):
            if fn_name.startswith("_"):
                continue
            fn = getattr(fc, fn_name, None)
            if not callable(fn):
                continue
            # Skip symbols re-exported from other modules (for example
            # torch.distributed.ReduceOp). Only entry points defined in
            # this module have __module__ == fc.__name__.
            if getattr(fn, "__module__", "") != fc.__name__:
                continue
            try:
                setattr(
                    fc,
                    fn_name,
                    roctx_wrapper(
                        fn, f"torch.distributed._functional_collectives.{fn_name}"
                    ),
                )
                wrapped.append(f"_functional_collectives.{fn_name}")
            except Exception as e:
                console_warning(
                    "torch trace",
                    f"Could not patch _functional_collectives.{fn_name}: {e}",
                )
    except ImportError:
        pass

    if wrapped:
        console_log(
            "torch trace",
            f"Wrapped {len(wrapped)} torch.distributed collectives with ROCTX markers",
        )


patch_distributed_collectives()

try:
    import torch.cuda

    torch.cuda.set_device = roctx_wrapper(
        torch.cuda.set_device, "torch.cuda.set_device"
    )
except ImportError:
    pass
except Exception as e:
    console_warning(
        "torch trace",
        f"Could not patch torch.cuda.set_device: {e}",
    )


# ---------------------------------------------------------------------------
# Dispatcher coverage via torch.utils._python_dispatch.TorchDispatchMode.
# ---------------------------------------------------------------------------


def next_dispatcher_index(op_name):
    """Return the next per-thread occurrence count for op_name."""
    counters = getattr(_thread_local, "dispatcher_counters", None)
    if counters is None:
        counters = {}
        _thread_local.dispatcher_counters = counters
    counters[op_name] = counters.get(op_name, 0) + 1
    return counters[op_name]


def warn_dispatcher_failure_once(phase, error):
    """Emit one console_warning per (thread, phase) and silently
    suppress subsequent failures. Dispatcher callbacks must not
    propagate exceptions, so failures cannot be surfaced any louder."""
    flag_attr = f"warned_dispatcher_failure_{phase}"
    if getattr(_thread_local, flag_attr, False):
        return
    setattr(_thread_local, flag_attr, True)
    try:
        console_warning(
            "torch trace",
            f"Dispatcher {phase} raised ({type(error).__name__}: {error}). "
            "Subsequent failures on this thread will be suppressed.",
        )
    except Exception:
        pass


def make_dispatcher_callbacks():
    """Return (start, end) callbacks for the TorchDispatchMode hook.
    start() is transactional: rangePush runs before any stack mutation,
    so a failed rangePush leaves all state unchanged."""

    def start(op_name):
        idx = next_dispatcher_index(op_name)
        location = resolve_user_caller_location()

        marker_stack = get_marker_stack()
        context_stack = get_context_stack()
        new_marker_entry = op_name
        new_context_entry = f"#{idx}@{location}"

        full_marker = (
            "/".join([*marker_stack, new_marker_entry])
            + ":"
            + "/".join([*context_stack, new_context_entry])
        )

        rangePush(full_marker)
        marker_stack.append(new_marker_entry)
        context_stack.append(new_context_entry)

    def end():
        marker_stack = get_marker_stack()
        context_stack = get_context_stack()
        try:
            rangePop()
        finally:
            if marker_stack:
                marker_stack.pop()
            if context_stack:
                context_stack.pop()

    return start, end


def try_install_torch_dispatch_mode(start, end):
    """Install a TorchDispatchMode whose __torch_dispatch__ invokes
    start() before and end() after every dispatched operator. Returns
    'torch_dispatch_mode' on success, or None if TorchDispatchMode
    could not be imported or activated on the current PyTorch."""
    global _active_dispatch_mode

    try:
        from torch.utils._python_dispatch import TorchDispatchMode
    except ImportError:
        return None

    class RoctxDispatchMode(TorchDispatchMode):
        def __torch_dispatch__(self, func, types, args=(), kwargs=None):
            kwargs = kwargs or {}

            try:
                raw = str(func)
            except Exception:
                raw = "<unknown_op>"
            # The structural wrappers and utils_analysis.py use
            # 'aten::mm.default' style. OpOverload may stringify either
            # way depending on the PyTorch version, so normalize the
            # namespace separator before emitting.
            if "::" in raw:
                op_name = raw
            else:
                op_name = raw.replace(".", "::", 1)

            pushed = False
            try:
                start(op_name)
                pushed = True
            except Exception as e:
                warn_dispatcher_failure_once("torch_dispatch_mode_start", e)

            try:
                return func(*args, **kwargs)
            finally:
                if pushed:
                    try:
                        end()
                    except Exception as e:
                        warn_dispatcher_failure_once("torch_dispatch_mode_end", e)

    try:
        mode = RoctxDispatchMode()
        mode.__enter__()
    except Exception as e:
        console_warning(
            "torch trace",
            f"TorchDispatchMode activation failed: {e}",
        )
        return None

    _active_dispatch_mode = mode
    return "torch_dispatch_mode"


def install_dispatcher_hook():
    """Activate dispatcher-level operator coverage via
    TorchDispatchMode. Returns 'torch_dispatch_mode' on success or
    'none' if no hook could be installed."""
    start, end = make_dispatcher_callbacks()

    tdm_tag = try_install_torch_dispatch_mode(start, end)
    if tdm_tag is not None:
        console_log(
            "torch trace",
            "Operator coverage: TorchDispatchMode (covers every dispatched "
            "ATen op, including tensor methods, operator overloads, and "
            "autograd backward kernels).",
        )
        return tdm_tag

    console_warning(
        "torch trace",
        "TorchDispatchMode is not importable on this PyTorch; "
        "individual aten ops will not be instrumented.",
    )
    return "none"


# ---------------------------------------------------------------------------
# Per-call structural wrappers installed from __main__.
# ---------------------------------------------------------------------------


def install_tensor_backward_wrapper():
    """Wrap torch.Tensor.backward. The call boundary itself is not a
    dispatched operator; per-operator backward dispatches issued by the
    autograd engine are captured separately by TorchDispatchMode."""
    original_backward = torch.Tensor.backward
    backward_counter = {"count": 0}

    def backward_with_roctx(self, *args, **kwargs):
        backward_counter["count"] += 1
        location = resolve_user_caller_location()

        marker_stack = get_marker_stack()
        context_stack = get_context_stack()
        marker_stack.append("torch.Tensor.backward")
        context_stack.append(f"#{backward_counter['count']}@{location}")
        full_marker_name = "/".join(marker_stack) + ":" + "/".join(context_stack)

        rangePush(full_marker_name)
        try:
            return original_backward(self, *args, **kwargs)
        finally:
            rangePop()
            marker_stack.pop()
            context_stack.pop()

    torch.Tensor.backward = backward_with_roctx
    console_log("Wrapped torch.Tensor.backward with ROCTX markers")


def wrap_method_on_subclasses(base_class, method_name, wrapper_factory):
    """Wrap method_name on every class in base_class's hierarchy that
    defines it, using wrapper_factory(original_method).

    Each definition is wrapped at most once: a subclass that inherits
    method_name shares the wrap installed on its defining ancestor.
    Currently-imported subclasses are wrapped eagerly; later subclasses
    are wrapped on first instantiation via a hook on base_class.__init__
    (subclasses overriding __init__ must call super().__init__()).
    """
    wrapped_classes = set()

    def wrap_class(cls):
        if cls in wrapped_classes:
            return
        wrapped_classes.add(cls)
        try:
            for ancestor in cls.__mro__:
                if method_name in ancestor.__dict__:
                    fn = ancestor.__dict__[method_name]
                    if not getattr(fn, "_roctx_wrapped", False):
                        wrapped_fn = wrapper_factory(fn)
                        wrapped_fn._roctx_wrapped = True
                        setattr(ancestor, method_name, wrapped_fn)
                    break
        except Exception as e:
            console_warning(
                "torch trace",
                f"Failed to wrap {cls.__name__}.{method_name}: {e}",
            )

    def walk_existing(cls):
        for sub in cls.__subclasses__():
            wrap_class(sub)
            walk_existing(sub)

    walk_existing(base_class)
    wrap_class(base_class)

    original_init = base_class.__init__

    def init_hook(self, *args, **kwargs):
        cls = type(self)
        if cls not in wrapped_classes:
            wrap_class(cls)
        return original_init(self, *args, **kwargs)

    base_class.__init__ = init_hook


def inject_roctx_into_optimizer():
    """Wrap step() on every torch.optim optimizer with ROCTX markers.
    Concrete optimizers redefine step() on their own subclass, so the
    wrap must target each subclass's definition rather than the base
    Optimizer.step; see wrap_method_on_subclasses for the mechanism."""
    from torch.optim import Optimizer

    def make_step_wrapper(original_step):
        def step_with_roctx(self, *args, **kwargs):
            marker_stack = get_marker_stack()
            context_stack = get_context_stack()
            marker_stack.append(f"optimizer.{type(self).__name__}.step")

            location = resolve_user_caller_location()

            if not hasattr(self, "_roctx_step_call_count"):
                self._roctx_step_call_count = 0
            self._roctx_step_call_count += 1
            context_stack.append(f"#{self._roctx_step_call_count}@{location}")

            full_marker_name = "/".join(marker_stack) + ":" + "/".join(context_stack)
            rangePush(full_marker_name)
            try:
                return original_step(self, *args, **kwargs)
            finally:
                rangePop()
                marker_stack.pop()
                context_stack.pop()

        return step_with_roctx

    wrap_method_on_subclasses(Optimizer, "step", make_step_wrapper)
    console_log(
        "Wrapped optimizer.step() across torch.optim subclasses with ROCTX markers\n"
    )


def inject_roctx_into_model():
    """Wrap torch.nn.Module.__call__ so every module invocation is
    enclosed in a ROCTX range identified by the module's class name and
    a per-instance call counter. Wrapping __call__ rather than forward()
    ensures hooks registered on the module are also covered."""

    from torch import nn

    original_call = nn.Module.__call__

    # Each nn.Module instance carries its own call counter so different
    # layers do not share the same #N index sequence.
    def call_with_roctx(self, *args, **kwargs):
        class_name = self.__class__.__name__
        if not hasattr(self, "_roctx_call_count"):
            self._roctx_call_count = 0
        self._roctx_call_count += 1

        location = resolve_user_caller_location()

        marker_stack = get_marker_stack()
        context_stack = get_context_stack()
        marker_stack.append(f"nn.Module.{class_name}.forward")
        context_stack.append(f"#{self._roctx_call_count}@{location}")
        full_marker_name = "/".join(marker_stack) + ":" + "/".join(context_stack)

        rangePush(full_marker_name)
        try:
            return original_call(self, *args, **kwargs)
        finally:
            rangePop()
            marker_stack.pop()
            context_stack.pop()

    nn.Module.__call__ = call_with_roctx
    console_log("Wrapped nn.Module forward() with ROCTX markers\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        console_log("Usage: python inject_roctx.py <script.py> [script_args...]")
        sys.exit(1)

    target_script = sys.argv[1]
    script_args = sys.argv[2:]

    # Instrumentation must be installed before the target script is
    # imported; any operator dispatched during target-script import
    # would otherwise escape ROCTX coverage.
    install_dispatcher_hook()
    install_tensor_backward_wrapper()
    inject_roctx_into_optimizer()
    inject_roctx_into_model()

    console_log("=" * 70)
    console_log("Starting target script with ROCTX instrumentation...")
    console_log("=" * 70)

    # Rewrite sys.argv so the target script's argument parser observes
    # its own command line rather than this wrapper's invocation.
    sys.argv = [target_script] + script_args

    # Execute the target script under the '__main__' module name so its
    # `if __name__ == "__main__"` guards activate normally.
    spec = importlib.util.spec_from_file_location("__main__", target_script)
    module = importlib.util.module_from_spec(spec)
    sys.modules["__main__"] = module
    spec.loader.exec_module(module)
