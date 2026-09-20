#pragma once

#include <HalMemory.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

class ReaderPageCache {
 public:
  static constexpr size_t CAPACITY = 7;

  struct Key {
    int spineIndex = -1;
    int pageNumber = -1;
    int pageCount = 0;
    uint32_t settingsIdentity = 0;
    uint8_t orientation = 0;
    uint8_t flags = 0;

    bool operator==(const Key&) const = default;
  };

  bool begin(const size_t frameBytes) {
    if (frameBytes == 0 || frameBytes > std::numeric_limits<size_t>::max() / CAPACITY) return false;
    data = makePsramBuffer<uint8_t>(frameBytes * CAPACITY);
    if (!data) return false;
    frameSize = frameBytes;
    return true;
  }

  void clear() {
    for (auto& entry : entries) entry.valid = false;
    entryCount = 0;
  }

  bool restore(const Key& key, uint8_t* destination, const bool recordStats = true) {
    Entry* entry = find(key);
    if (!entry) {
      if (recordStats) misses++;
      return false;
    }
    std::memcpy(destination, slotData(*entry), frameSize);
    entry->lastUsed = ++clock;
    if (recordStats) hits++;
    return true;
  }

  bool contains(const Key& key) const {
    for (const auto& entry : entries) {
      if (entry.valid && entry.key == key) return true;
    }
    return false;
  }

  bool store(const Key& key, const uint8_t* source) {
    if (!data || !source) return false;

    Entry* target = find(key);
    if (!target) {
      for (auto& entry : entries) {
        if (!entry.valid) {
          target = &entry;
          entryCount++;
          break;
        }
      }
    }
    if (!target) {
      target = &entries[0];
      for (auto& entry : entries) {
        if (entry.lastUsed < target->lastUsed) target = &entry;
      }
      evictions++;
    }

    target->key = key;
    target->valid = true;
    target->lastUsed = ++clock;
    std::memcpy(slotData(*target), source, frameSize);
    pagesCached++;
    return true;
  }

  bool enabled() const { return data != nullptr; }
  size_t size() const { return entryCount; }
  size_t bytesUsed() const { return enabled() ? frameSize * CAPACITY : 0; }
  uint32_t hitCount() const { return hits; }
  uint32_t missCount() const { return misses; }
  uint32_t evictionCount() const { return evictions; }
  uint32_t pagesCachedCount() const { return pagesCached; }

 private:
  struct Entry {
    Key key;
    uint32_t lastUsed = 0;
    bool valid = false;
  };

  Entry* find(const Key& key) {
    for (auto& entry : entries) {
      if (entry.valid && entry.key == key) return &entry;
    }
    return nullptr;
  }

  uint8_t* slotData(const Entry& entry) {
    return data.get() + static_cast<size_t>(&entry - entries.data()) * frameSize;
  }

  PsramBuffer<uint8_t> data;
  std::array<Entry, CAPACITY> entries{};
  size_t frameSize = 0;
  size_t entryCount = 0;
  uint32_t clock = 0;
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint32_t evictions = 0;
  uint32_t pagesCached = 0;
};
