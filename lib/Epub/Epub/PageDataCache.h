#pragma once

#include <HalMemory.h>

#include <algorithm>
#include <cstdint>

// Serialized bytes only; Page and its nested allocations keep their existing owners.
class PageDataCache {
 public:
  static constexpr int PREVIOUS = 2;
  static constexpr int NEXT = 8;
  static constexpr int CAPACITY = PREVIOUS + 1 + NEXT;
  // ponytail: fixed 32 KiB slots; oversized pages use SD, add size classes if measured necessary.
  static constexpr size_t PAGE_BYTES = 32 * 1024;

  struct Entry {
    int page;
    uint32_t length;
    uint32_t loaded;
    uint32_t visibleTextOffset;
    uint8_t bytes[PAGE_BYTES];
  };

  bool begin() {
    if (!attempted) {
      attempted = true;
      if (HalMemory::hasPsram()) {
        state = makePsramBuffer<State>(1, true);
        clear();
      }
    }
    return enabled();
  }

  void clear() {
    if (!state) return;
    for (auto& entry : state[0].entries) entry.page = -1;
  }

  void discard(int page) {
    if (auto* entry = find(page)) entry->page = -1;
  }

  void setWindow(int currentPage, int pageCount) {
    if (!state) return;
    for (auto& entry : state[0].entries) {
      if (entry.page >= 0 &&
          (entry.page < currentPage - PREVIOUS || entry.page > currentPage + NEXT || entry.page >= pageCount)) {
        entry.page = -1;
      }
    }
  }

  int nextPage(int currentPage, int pageCount) const {
    if (!state) return -1;
    static constexpr int8_t OFFSETS[] = {0, 1, -1, 2, -2, 3, 4, 5, 6, 7, 8};
    for (const int offset : OFFSETS) {
      const int page = currentPage + offset;
      if (page >= 0 && page < pageCount && !find(page)) return page;
    }
    return -1;
  }

  Entry* find(int page) const {
    if (!state || page < 0) return nullptr;
    auto& entry = state[0].entries[page % CAPACITY];
    return entry.page == page ? &entry : nullptr;
  }

  Entry& prepare(int page) {
    auto& entry = state[0].entries[page % CAPACITY];
    entry.page = page;
    entry.length = 0;
    entry.loaded = 0;
    entry.visibleTextOffset = 0;
    return entry;
  }

  bool enabled() const { return state != nullptr; }
  size_t bytesUsed() const { return state ? sizeof(State) : 0; }

 private:
  struct State {
    Entry entries[CAPACITY];
  };
  PsramBuffer<State> state;
  bool attempted = false;
};
