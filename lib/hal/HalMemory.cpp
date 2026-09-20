#include "HalMemory.h"

#include <Logging.h>
#include <esp_heap_caps.h>

#include <cstdint>

namespace {
constexpr uint32_t PSRAM_CAPABILITIES = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

HalMemory::HeapStats readHeapStats(uint32_t capabilities) {
  return {heap_caps_get_free_size(capabilities), heap_caps_get_total_size(capabilities),
          heap_caps_get_minimum_free_size(capabilities), heap_caps_get_largest_free_block(capabilities)};
}
}  // namespace

HalMemory::HeapStats HalMemory::getDefaultHeap() { return readHeapStats(MALLOC_CAP_DEFAULT); }

HalMemory::HeapStats HalMemory::getInternalHeap() { return readHeapStats(MALLOC_CAP_INTERNAL); }

HalMemory::HeapStats HalMemory::getPsramHeap() { return readHeapStats(MALLOC_CAP_SPIRAM); }

bool HalMemory::hasPsram() { return heap_caps_get_total_size(PSRAM_CAPABILITIES) != 0; }

void* HalMemory::allocatePsram(size_t bytes, bool zeroInitialize) {
  if (bytes == 0) {
    return nullptr;
  }
  if (!hasPsram()) {
    LOG_ERR("MEM", "PSRAM unavailable: requested %zu bytes", bytes);
    return nullptr;
  }
  void* ptr =
      zeroInitialize ? heap_caps_calloc(1, bytes, PSRAM_CAPABILITIES) : heap_caps_malloc(bytes, PSRAM_CAPABILITIES);
  if (!ptr) {
    LOG_ERR("MEM", "PSRAM allocation failed: %zu bytes", bytes);
  }
  return ptr;
}

void HalMemory::freePsram(void* ptr) { heap_caps_free(ptr); }
