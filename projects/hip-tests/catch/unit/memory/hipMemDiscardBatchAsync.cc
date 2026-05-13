/*
 * Copyright (c) Advanced Micro Devices, Inc., or its affiliates.
 *
 * SPDX-License-Identifier: MIT
 */

#include <hip_test_common.hh>
#include <hip_test_defgroups.hh>
#include <utils.hh>
#include <vector>

/**
 * @addtogroup hipMemDiscardBatchAsync hipMemDiscardBatchAsync
 * @{
 * @ingroup MemoryTest
 * `hipError_t hipMemDiscardBatchAsync(void** dev_ptrs, size_t* sizes, size_t count,
 *                                     unsigned long long flags, hipStream_t stream)`
 * `hipError_t hipDrvMemDiscardBatchAsync(hipDeviceptr_t* dptrs, size_t* sizes, size_t count,
 *                                        unsigned long long flags, hipStream_t stream)`
 *
 * Discard a batch of managed memory ranges asynchronously.
 */

// Helper: check if device supports managed memory + concurrent managed access (HMM/XNACK)
static bool HmmSupported(int device = 0) {
  return DeviceAttributesSupport(device,
    hipDeviceAttributeManagedMemory,
    hipDeviceAttributeConcurrentManagedAccess);
}

// =====================================================================================
// Negative Tests — run on all platforms, no HMM guard needed
// =====================================================================================

/**
 * Test Description
 * ------------------------
 * - Verify parameter validation for hipMemDiscardBatchAsync and hipDrvMemDiscardBatchAsync.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_NegativeTests) {
  constexpr size_t kAllocSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  // Allocate managed memory for tests that need valid pointers
  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kAllocSize));

  void* dev_ptrs[2] = {managed_ptr, managed_ptr};
  size_t sizes[2] = {kAllocSize, kAllocSize};

  SECTION("NullDevPtrs") {
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(nullptr, sizes, 1, 0, stream),
        hipErrorInvalidValue);
    HIP_CHECK_ERROR(
        hipDrvMemDiscardBatchAsync(nullptr, sizes, 1, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("NullSizes") {
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(dev_ptrs, nullptr, 1, 0, stream),
        hipErrorInvalidValue);
    hipDeviceptr_t dptrs[1] = {reinterpret_cast<hipDeviceptr_t>(managed_ptr)};
    HIP_CHECK_ERROR(
        hipDrvMemDiscardBatchAsync(dptrs, nullptr, 1, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("ZeroCount") {
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(dev_ptrs, sizes, 0, 0, stream),
        hipErrorInvalidValue);
    hipDeviceptr_t dptrs[1] = {reinterpret_cast<hipDeviceptr_t>(managed_ptr)};
    HIP_CHECK_ERROR(
        hipDrvMemDiscardBatchAsync(dptrs, sizes, 0, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("NonZeroFlags") {
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(dev_ptrs, sizes, 1, 1, stream),
        hipErrorInvalidValue);
    hipDeviceptr_t dptrs[1] = {reinterpret_cast<hipDeviceptr_t>(managed_ptr)};
    HIP_CHECK_ERROR(
        hipDrvMemDiscardBatchAsync(dptrs, sizes, 1, 1, stream),
        hipErrorInvalidValue);
  }

  SECTION("NullStream") {
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(dev_ptrs, sizes, 1, 0, nullptr),
        hipErrorInvalidValue);
    hipDeviceptr_t dptrs[1] = {reinterpret_cast<hipDeviceptr_t>(managed_ptr)};
    HIP_CHECK_ERROR(
        hipDrvMemDiscardBatchAsync(dptrs, sizes, 1, 0, nullptr),
        hipErrorInvalidValue);
  }

  SECTION("NullPtrInArray") {
    void* ptrs_with_null[2] = {managed_ptr, nullptr};
    size_t two_sizes[2] = {kAllocSize, kAllocSize};
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(ptrs_with_null, two_sizes, 2, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("ZeroSizeInArray") {
    void* two_ptrs[2] = {managed_ptr, managed_ptr};
    size_t sizes_with_zero[2] = {kAllocSize, 0};
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(two_ptrs, sizes_with_zero, 2, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("SizeExceedsAllocation") {
    size_t oversized[1] = {kAllocSize + 1};
    HIP_CHECK_ERROR(
        hipMemDiscardBatchAsync(dev_ptrs, oversized, 1, 0, stream),
        hipErrorInvalidValue);
  }

  SECTION("NonManagedMemory") {
    void* device_ptr = nullptr;
    HIP_CHECK(hipMalloc(&device_ptr, kAllocSize));
    void* non_managed_ptrs[1] = {device_ptr};
    size_t non_managed_sizes[1] = {kAllocSize};
    // Non-managed memory should fail if pageable memory access not supported,
    // or succeed if it is. Either way it should not crash.
    hipError_t err = hipMemDiscardBatchAsync(non_managed_ptrs, non_managed_sizes,
                                             1, 0, stream);
    // Accept success (pageable access supported), not-supported (capability gap),
    // or invalid-value (non-managed memory rejected, e.g. on NVIDIA)
    REQUIRE((err == hipSuccess || err == hipErrorNotSupported || err == hipErrorInvalidValue));
    HIP_CHECK(hipFree(device_ptr));
  }

  SECTION("SystemAllocatedMemory") {
    void* sys_ptr = malloc(kAllocSize);
    REQUIRE(sys_ptr != nullptr);
    void* sys_ptrs[1] = {sys_ptr};
    size_t sys_sizes[1] = {kAllocSize};
    // System-allocated memory (malloc) should succeed if pageable memory access
    // is supported, or fail with hipErrorNotSupported otherwise. Must not crash.
    hipError_t err = hipMemDiscardBatchAsync(sys_ptrs, sys_sizes, 1, 0, stream);
    REQUIRE((err == hipSuccess || err == hipErrorNotSupported));
    free(sys_ptr);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

// =====================================================================================
// Functional Tests — require HMM/XNACK support
// =====================================================================================

/**
 * Test Description
 * ------------------------
 * - Basic single discard: allocate managed memory, write values, discard,
 *   write new values, read back to verify memory is still usable after discard.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_SingleDiscard) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));

  // Initialize with known values
  memset(managed_ptr, 0xAB, kSize);

  // Discard via runtime API
  void* ptrs[1] = {managed_ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Write new values — this "undoes" the discard per CUDA semantics
  memset(managed_ptr, 0xCD, kSize);

  // Verify new values are readable
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(bytes[i] == 0xCD);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Multiple discards in a single batch with varying sizes.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_MultipleDiscards) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kCount = 4;
  constexpr size_t kSizes[kCount] = {4096, 65536, 1024 * 1024, 16};
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptrs[kCount] = {};
  size_t sizes[kCount];
  for (size_t i = 0; i < kCount; i++) {
    HIP_CHECK(hipMallocManaged(&managed_ptrs[i], kSizes[i]));
    memset(managed_ptrs[i], static_cast<int>(0xAA + i), kSizes[i]);
    sizes[i] = kSizes[i];
  }

  HIP_CHECK(hipMemDiscardBatchAsync(managed_ptrs, sizes, kCount, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Write and verify each allocation is still usable
  for (size_t i = 0; i < kCount; i++) {
    unsigned char fill_val = static_cast<unsigned char>(0x10 + i);
    memset(managed_ptrs[i], fill_val, kSizes[i]);
    auto* bytes = static_cast<unsigned char*>(managed_ptrs[i]);
    for (size_t j = 0; j < kSizes[i]; j++) {
      INFO("Allocation " << i << " byte " << j);
      REQUIRE(bytes[j] == fill_val);
    }
  }

  for (size_t i = 0; i < kCount; i++) {
    HIP_CHECK(hipFree(managed_ptrs[i]));
  }
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Large batch count stress test with 64 operations.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_LargeCount) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kCount = 64;
  constexpr size_t kEachSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  std::vector<void*> managed_ptrs(kCount);
  std::vector<size_t> sizes(kCount, kEachSize);

  for (size_t i = 0; i < kCount; i++) {
    HIP_CHECK(hipMallocManaged(&managed_ptrs[i], kEachSize));
    memset(managed_ptrs[i], 0xFF, kEachSize);
  }

  HIP_CHECK(hipMemDiscardBatchAsync(managed_ptrs.data(), sizes.data(), kCount, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Verify all allocations are still usable after discard
  for (size_t i = 0; i < kCount; i++) {
    unsigned char val = static_cast<unsigned char>(i & 0xFF);
    memset(managed_ptrs[i], val, kEachSize);
    auto* bytes = static_cast<unsigned char*>(managed_ptrs[i]);
    REQUIRE(bytes[0] == val);
    REQUIRE(bytes[kEachSize - 1] == val);
  }

  for (size_t i = 0; i < kCount; i++) {
    HIP_CHECK(hipFree(managed_ptrs[i]));
  }
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Discard then prefetch: validates that prefetch "undoes" discard per CUDA semantics.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_DiscardThenPrefetch) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));
  memset(managed_ptr, 0xBB, kSize);

  // Discard
  void* ptrs[1] = {managed_ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));

  // Prefetch to device 0 — should "undo" the discard
  int device;
  HIP_CHECK(hipGetDevice(&device));
  HIP_CHECK(hipMemPrefetchAsync(managed_ptr, kSize, device, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Memory should be accessible — write and read back
  memset(managed_ptr, 0xCC, kSize);
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(bytes[i] == 0xCC);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Stream ordering: write → discard → write on same stream,
 *   verify final values are correct (discard doesn't reorder with writes).
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_StreamOrdering) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));

  // Write initial values
  memset(managed_ptr, 0x11, kSize);

  // Discard on stream
  void* ptrs[1] = {managed_ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));

  // Write new values on same stream (after discard completes in stream order)
  HIP_CHECK(hipStreamSynchronize(stream));
  memset(managed_ptr, 0x22, kSize);

  // Verify the new values
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(bytes[i] == 0x22);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Discard a sub-range of a larger managed allocation.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_SubrangeDiscard) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kTotalSize = 65536;
  constexpr size_t kOffset = 4096;
  constexpr size_t kDiscardSize = 8192;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kTotalSize));
  memset(managed_ptr, 0xDD, kTotalSize);

  // Discard only a sub-range
  void* sub_ptr = static_cast<char*>(managed_ptr) + kOffset;
  void* ptrs[1] = {sub_ptr};
  size_t sizes[1] = {kDiscardSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Bytes before the discarded range should be intact
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kOffset; i++) {
    REQUIRE(bytes[i] == 0xDD);
  }

  // Bytes after the discarded range should be intact
  for (size_t i = kOffset + kDiscardSize; i < kTotalSize; i++) {
    REQUIRE(bytes[i] == 0xDD);
  }

  // Discarded sub-range should still be writable
  memset(sub_ptr, 0xEE, kDiscardSize);
  auto* sub_bytes = static_cast<unsigned char*>(sub_ptr);
  for (size_t i = 0; i < kDiscardSize; i++) {
    REQUIRE(sub_bytes[i] == 0xEE);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Same pointer appears twice in the batch — should succeed without crash.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_SamePointerTwice) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));
  memset(managed_ptr, 0xAA, kSize);

  void* ptrs[2] = {managed_ptr, managed_ptr};
  size_t sizes[2] = {kSize, kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 2, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Should still be usable
  memset(managed_ptr, 0xBB, kSize);
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  REQUIRE(bytes[0] == 0xBB);
  REQUIRE(bytes[kSize - 1] == 0xBB);

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Minimal size discard (1 byte).
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_MinimalSize) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kAllocSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kAllocSize));

  void* ptrs[1] = {managed_ptr};
  size_t sizes[1] = {1};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Should still be usable
  auto* bytes = static_cast<unsigned char*>(managed_ptr);
  bytes[0] = 0x42;
  REQUIRE(bytes[0] == 0x42);

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 * - Driver API parity: run the same discard with both hipMemDiscardBatchAsync
 *   and hipDrvMemDiscardBatchAsync, verify identical behavior.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_DrvApiParity) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  // Test with runtime API
  void* managed_ptr1 = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr1, kSize));
  memset(managed_ptr1, 0xAA, kSize);
  {
    void* ptrs[1] = {managed_ptr1};
    size_t sizes[1] = {kSize};
    HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
    HIP_CHECK(hipStreamSynchronize(stream));
  }
  memset(managed_ptr1, 0x11, kSize);

  // Test with driver API
  void* managed_ptr2 = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr2, kSize));
  memset(managed_ptr2, 0xBB, kSize);
  {
    hipDeviceptr_t dptrs[1] = {reinterpret_cast<hipDeviceptr_t>(managed_ptr2)};
    size_t sizes[1] = {kSize};
    HIP_CHECK(hipDrvMemDiscardBatchAsync(dptrs, sizes, 1, 0, stream));
    HIP_CHECK(hipStreamSynchronize(stream));
  }
  memset(managed_ptr2, 0x22, kSize);

  // Both should be usable identically
  auto* bytes1 = static_cast<unsigned char*>(managed_ptr1);
  auto* bytes2 = static_cast<unsigned char*>(managed_ptr2);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(bytes1[i] == 0x11);
    REQUIRE(bytes2[i] == 0x22);
  }

  HIP_CHECK(hipFree(managed_ptr1));
  HIP_CHECK(hipFree(managed_ptr2));
  HIP_CHECK(hipStreamDestroy(stream));
}
/**
 * Test Description
 * ------------------------
 * - Content-after-discard test: fill managed memory with a known pattern,
 *   discard it, then immediately read back. Per the CUDA/HIP spec, content
 *   after discard is undefined — the read must not fault or crash.
 *   Then write new values and verify they stick.
 * Test source
 * ------------------------
 * - catch/unit/memory/hipMemDiscardBatchAsync.cc
 * Test requirements
 * ------------------------
 *  - HIP_VERSION >= 7.2
 *  - Device supports managed memory and concurrent managed access
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_ContentAfterDiscard) {
  if (!HmmSupported()) {
    HIP_SKIP_TEST("HMM/managed memory not supported");
    return;
  }

  constexpr size_t kSize = 65536;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));

  // Fill with known pattern
  memset(managed_ptr, 0xAB, kSize);

  // Discard
  void* ptrs[1] = {managed_ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Read back immediately — content is undefined per spec.
  // We do NOT assert specific values, but the access must not fault.
  auto* bytes = static_cast<volatile unsigned char*>(managed_ptr);
  volatile unsigned char sink = 0;
  for (size_t i = 0; i < kSize; i++) {
    sink = bytes[i];  // must not segfault
  }
  (void)sink;

  // Now write new values and verify they stick
  memset(managed_ptr, 0xCD, kSize);
  auto* check = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(check[i] == 0xCD);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

// =============================================================================
// Pageable Memory Functional Tests — require PageableMemoryAccess (MI300+ / XNACK)
// AMD-only: CUDA rejects device-only and pinned memory for discard even when
// PageableMemoryAccess is supported. On AMD with XNACK, all memory types
// participate in HMM page migration, so discard works on any allocation.
// =============================================================================

#if !HT_NVIDIA

static bool PageableMemoryAccessSupported(int device = 0) {
  int pageable = 0;
  hipDeviceGetAttribute(&pageable, hipDeviceAttributePageableMemoryAccess, device);
  return pageable != 0;
}

/**
 * Test Description
 * ------------------------
 *   Verify hipMemDiscardBatchAsync works with hipMalloc (device-only) memory
 *   when PageableMemoryAccess is supported. The GPU can access any page, so
 *   discard should succeed even for non-managed device allocations.
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_PageableDeviceMemory) {
  if (!HmmSupported()) {
    HipTest::HIP_SKIP_TEST("HMM not supported");
    return;
  }
  if (!PageableMemoryAccessSupported()) {
    HipTest::HIP_SKIP_TEST("PageableMemoryAccess not supported");
    return;
  }

  constexpr size_t kSize = 65536;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* ptr = nullptr;
  HIP_CHECK(hipMalloc(&ptr, kSize));
  HIP_CHECK(hipMemset(ptr, 0xAB, kSize));
  HIP_CHECK(hipStreamSynchronize(stream));

  void* ptrs[1] = {ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Write new data and verify
  HIP_CHECK(hipMemset(ptr, 0xCD, kSize));
  HIP_CHECK(hipStreamSynchronize(stream));

  std::vector<unsigned char> host_buf(kSize);
  HIP_CHECK(hipMemcpy(host_buf.data(), ptr, kSize, hipMemcpyDeviceToHost));
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(host_buf[i] == 0xCD);
  }

  HIP_CHECK(hipFree(ptr));
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 *   Verify hipMemDiscardBatchAsync works with system-allocated (malloc) memory
 *   when PageableMemoryAccess is supported.
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_PageableSystemMemory) {
  if (!HmmSupported()) {
    HipTest::HIP_SKIP_TEST("HMM not supported");
    return;
  }
  if (!PageableMemoryAccessSupported()) {
    HipTest::HIP_SKIP_TEST("PageableMemoryAccess not supported");
    return;
  }

  constexpr size_t kSize = 65536;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* ptr = malloc(kSize);
  REQUIRE(ptr != nullptr);
  memset(ptr, 0xAB, kSize);

  void* ptrs[1] = {ptr};
  size_t sizes[1] = {kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 1, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Write and verify
  memset(ptr, 0xCD, kSize);
  auto* bytes = static_cast<unsigned char*>(ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(bytes[i] == 0xCD);
  }

  free(ptr);
  HIP_CHECK(hipStreamDestroy(stream));
}

/**
 * Test Description
 * ------------------------
 *   Verify hipMemDiscardBatchAsync works with a mixed batch of managed, device,
 *   and system-allocated memory when PageableMemoryAccess is supported.
 */
HIP_TEST_CASE(Unit_hipMemDiscardBatchAsync_PageableMixedBatch) {
  if (!HmmSupported()) {
    HipTest::HIP_SKIP_TEST("HMM not supported");
    return;
  }
  if (!PageableMemoryAccessSupported()) {
    HipTest::HIP_SKIP_TEST("PageableMemoryAccess not supported");
    return;
  }

  constexpr size_t kSize = 4096;
  hipStream_t stream;
  HIP_CHECK(hipStreamCreate(&stream));

  void* managed_ptr = nullptr;
  void* device_ptr = nullptr;
  void* system_ptr = malloc(kSize);
  REQUIRE(system_ptr != nullptr);
  HIP_CHECK(hipMallocManaged(&managed_ptr, kSize));
  HIP_CHECK(hipMalloc(&device_ptr, kSize));

  memset(managed_ptr, 0xAA, kSize);
  memset(system_ptr, 0xBB, kSize);
  HIP_CHECK(hipMemset(device_ptr, 0xCC, kSize));
  HIP_CHECK(hipStreamSynchronize(stream));

  void* ptrs[3] = {managed_ptr, device_ptr, system_ptr};
  size_t sizes[3] = {kSize, kSize, kSize};
  HIP_CHECK(hipMemDiscardBatchAsync(ptrs, sizes, 3, 0, stream));
  HIP_CHECK(hipStreamSynchronize(stream));

  // Write new data
  memset(managed_ptr, 0x11, kSize);
  HIP_CHECK(hipMemset(device_ptr, 0x22, kSize));
  memset(system_ptr, 0x33, kSize);
  HIP_CHECK(hipStreamSynchronize(stream));

  // Verify managed
  auto* mb = static_cast<unsigned char*>(managed_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(mb[i] == 0x11);
  }

  // Verify device
  std::vector<unsigned char> dev_buf(kSize);
  HIP_CHECK(hipMemcpy(dev_buf.data(), device_ptr, kSize, hipMemcpyDeviceToHost));
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(dev_buf[i] == 0x22);
  }

  // Verify system
  auto* sb = static_cast<unsigned char*>(system_ptr);
  for (size_t i = 0; i < kSize; i++) {
    REQUIRE(sb[i] == 0x33);
  }

  HIP_CHECK(hipFree(managed_ptr));
  HIP_CHECK(hipFree(device_ptr));
  free(system_ptr);
  HIP_CHECK(hipStreamDestroy(stream));
}

#endif  // !HT_NVIDIA

/**
 * End doxygen group MemoryTest.
 * @}
 */
