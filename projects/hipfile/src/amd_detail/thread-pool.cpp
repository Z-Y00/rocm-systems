/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include "thread-pool.h"

#include <limits>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
#include <stdexcept>
#include <thread>
#include <utility>

namespace hipFile {

namespace {

std::size_t
validateThreadCount(std::size_t thread_count)
{
    if (thread_count == 0) {
        throw std::invalid_argument("Thread pool thread count must be greater than zero");
    }
    if (thread_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Thread pool thread count exceeds TBB task arena limits");
    }
    return thread_count;
}

int
taskArenaConcurrency(std::size_t thread_count)
{
    return static_cast<int>(validateThreadCount(thread_count));
}

}

struct ThreadPool::Impl {
    explicit Impl(std::size_t _thread_count)
        : thread_count{validateThreadCount(_thread_count)}, arena{taskArenaConcurrency(thread_count)},
          tasks{}
    {
    }

    std::size_t     thread_count;
    tbb::task_arena arena;
    tbb::task_group tasks;
};

ThreadPool::ThreadPool() : impl{std::make_unique<Impl>(hardwareThreadCount())}
{
}

ThreadPool::~ThreadPool()
{
    try {
        wait();
    }
    catch (...) {
        // Explicit wait() preserves task failures. Destructors cannot report them.
    }
}

void
ThreadPool::enqueue(std::function<void()> work)
{
    if (!work) {
        throw std::invalid_argument("Thread pool work item cannot be empty");
    }

    Impl &pool = *impl;
    pool.arena.execute([&pool, task = std::move(work)]() mutable { pool.tasks.run(std::move(task)); });
}

void
ThreadPool::wait()
{
    Impl &pool = *impl;
    pool.arena.execute([&pool]() { pool.tasks.wait(); });
}

std::size_t
ThreadPool::threadCount() const noexcept
{
    return impl->thread_count;
}

std::size_t
ThreadPool::hardwareThreadCount() noexcept
{
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    return hardware_threads == 0 ? 1 : static_cast<std::size_t>(hardware_threads);
}

}
