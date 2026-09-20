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
        if (entry.length && entry.loaded == entry.length) state[0].evictions++;
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
    if (entry.page >= 0 && entry.page != page && entry.length && entry.loaded == entry.length) state[0].evictions++;
    entry.page = page;
    entry.length = 0;
    entry.loaded = 0;
    entry.visibleTextOffset = 0;
    return entry;
  }

  void recordLoad(bool hit) {
    if (!state) return;
    if (hit)
      state[0].hits++;
    else
      state[0].misses++;
  }

  bool enabled() const { return state != nullptr; }
  size_t bytesUsed() const { return state ? sizeof(State) : 0; }
  size_t payloadBytes() const {
    size_t bytes = 0;
    if (state) {
      for (const auto& entry : state[0].entries) {
        if (entry.page >= 0 && entry.loaded == entry.length) bytes += entry.length;
      }
    }
    return bytes;
  }
  uint32_t hits() const { return state ? state[0].hits : 0; }
  uint32_t misses() const { return state ? state[0].misses : 0; }
  uint32_t evictions() const { return state ? state[0].evictions : 0; }

 private:
  struct State {
    Entry entries[CAPACITY];
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
  };
  PsramBuffer<State> state;
  bool attempted = false;
};
