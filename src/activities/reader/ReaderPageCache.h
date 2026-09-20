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

  bool restore(const Key& key, uint8_t* destination) {
    Entry* entry = find(key);
    if (!entry) return false;
    std::memcpy(destination, slotData(*entry), frameSize);
    entry->lastUsed = ++clock;
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
    }

    target->key = key;
    target->valid = true;
    target->lastUsed = ++clock;
    std::memcpy(slotData(*target), source, frameSize);
    return true;
  }

  bool enabled() const { return data != nullptr; }
  size_t size() const { return entryCount; }
  size_t bytesUsed() const { return enabled() ? frameSize * CAPACITY : 0; }

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
};
