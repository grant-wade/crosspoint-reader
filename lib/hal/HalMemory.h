#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

class HalMemory {
 public:
  struct HeapStats {
    size_t freeBytes;
    size_t totalBytes;
    size_t minFreeBytes;
    size_t largestBlockBytes;
  };

  // Default-capability memory includes PSRAM when registered with the allocator.
  static HeapStats getDefaultHeap();
  static HeapStats getInternalHeap();
  static HeapStats getPsramHeap();

  static bool hasPsram();
  // PSRAM only, with no internal-heap fallback. Zero bytes or unavailable memory returns nullptr.
  [[nodiscard]] static void* allocatePsram(size_t bytes, bool zeroInitialize = false);
  static void freePsram(void* ptr);  // Accepts nullptr; pair only with allocatePsram().
};

// Raw storage only. ESP-IDF heap_caps_malloc guarantees 4-byte alignment.
template <typename T>
concept PsramElement = std::is_trivial_v<T> && std::is_standard_layout_v<T> && !std::is_const_v<T> &&
                       !std::is_volatile_v<T> && !std::is_array_v<T> && alignof(T) <= alignof(uint32_t);

struct PsramDeleter {
  void operator()(void* ptr) const noexcept { HalMemory::freePsram(ptr); }
};

template <PsramElement T>
using PsramBuffer = std::unique_ptr<T[], PsramDeleter>;

// zeroInitialize clears bytes; it does not perform C++ value initialization.
template <PsramElement T>
[[nodiscard]] PsramBuffer<T> makePsramBuffer(size_t count, bool zeroInitialize = false) {
  if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
    return nullptr;
  }
  return PsramBuffer<T>(static_cast<T*>(HalMemory::allocatePsram(count * sizeof(T), zeroInitialize)));
}
