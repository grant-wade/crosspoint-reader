#include <HalMemory.h>
#include <esp_heap_caps.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

struct Pod {
  uint32_t value;
  uint8_t flag;
};
struct NonTrivial {
  ~NonTrivial() {}
};
struct alignas(8) OverAligned {
  uint8_t value;
};

template <typename T>
concept CanAllocatePsram = requires { makePsramBuffer<T>(1); };

static_assert(CanAllocatePsram<uint8_t> && CanAllocatePsram<uint32_t> && CanAllocatePsram<Pod>);
static_assert(!CanAllocatePsram<std::string> && !CanAllocatePsram<NonTrivial> && !CanAllocatePsram<OverAligned>);
static_assert(!CanAllocatePsram<const uint8_t> && !CanAllocatePsram<volatile uint8_t>);
static_assert(!CanAllocatePsram<void> && !CanAllocatePsram<uint8_t[4]>);
static_assert(!std::is_copy_constructible_v<PsramBuffer<Pod>>);
static_assert(std::is_nothrow_move_constructible_v<PsramBuffer<Pod>>);

static bool psramAvailable = false;
static bool failAllocation = false;
static size_t allocationCalls = 0;
static size_t liveAllocations = 0;
static size_t lastBytes = 0;

size_t heap_caps_get_total_size(uint32_t caps) {
  return (caps & MALLOC_CAP_SPIRAM) && psramAvailable ? 8 * 1024 * 1024 : 0;
}
size_t heap_caps_get_free_size(uint32_t caps) { return heap_caps_get_total_size(caps); }
size_t heap_caps_get_minimum_free_size(uint32_t caps) { return heap_caps_get_total_size(caps); }
size_t heap_caps_get_largest_free_block(uint32_t caps) { return heap_caps_get_total_size(caps); }

void* heap_caps_malloc(size_t bytes, uint32_t caps) {
  assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ++allocationCalls;
  lastBytes = bytes;
  if (!psramAvailable || failAllocation) {
    return nullptr;
  }
  void* ptr = std::malloc(bytes);
  assert(ptr);
  std::memset(ptr, 0xA5, bytes);
  ++liveAllocations;
  return ptr;
}
void* heap_caps_calloc(size_t count, size_t bytes, uint32_t caps) {
  assert(count == 1);
  void* ptr = heap_caps_malloc(bytes, caps);
  if (ptr) {
    std::memset(ptr, 0, bytes);
  }
  return ptr;
}
void heap_caps_free(void* ptr) {
  if (ptr) {
    assert(liveAllocations > 0);
    --liveAllocations;
  }
  std::free(ptr);
}

int main() {
  assert(!HalMemory::hasPsram());
  assert(!HalMemory::allocatePsram(1024 * 1024));
  assert(!makePsramBuffer<uint8_t>(1024 * 1024, true));
  assert(allocationCalls == 0);
  HalMemory::freePsram(nullptr);

  psramAvailable = true;
  assert(HalMemory::hasPsram());
  assert(!HalMemory::allocatePsram(0));
  assert(!makePsramBuffer<uint8_t>(0, true));
  assert(!makePsramBuffer<uint32_t>(std::numeric_limits<size_t>::max() / sizeof(uint32_t) + 1));
  assert(allocationCalls == 0);
  failAllocation = true;
  assert(!makePsramBuffer<Pod>(4));
  assert(!makePsramBuffer<Pod>(4, true));
  assert(liveAllocations == 0);
  failAllocation = false;

  for (int run = 0; run < 50; ++run) {
    {
      auto buffer = makePsramBuffer<uint32_t>(1024 * 1024 / sizeof(uint32_t), true);
      assert(buffer && lastBytes == 1024 * 1024 && liveAllocations == 1);
      for (size_t i = 0; i < lastBytes / sizeof(uint32_t); ++i) {
        assert(buffer[i] == 0);
        buffer[i] = static_cast<uint32_t>(i) ^ 0xA5C39E71U;
      }
      auto moved = std::move(buffer);
      assert(!buffer && liveAllocations == 1);
      for (size_t i = 0; i < lastBytes / sizeof(uint32_t); ++i) {
        assert(moved[i] == (static_cast<uint32_t>(i) ^ 0xA5C39E71U));
      }
      auto replacement = makePsramBuffer<uint8_t>(16);
      assert(replacement && replacement[0] == 0xA5 && liveAllocations == 2);
      replacement.reset();
      assert(liveAllocations == 1);
    }
    assert(liveAllocations == 0);
  }
  auto pod = makePsramBuffer<Pod>(2);
  assert(pod && lastBytes == 2 * sizeof(Pod));
  pod[1].value = 42;
  auto* released = pod.release();
  assert(!pod && released[1].value == 42);
  HalMemory::freePsram(released);
  assert(liveAllocations == 0);
}
