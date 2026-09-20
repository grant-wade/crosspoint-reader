#pragma once

#include <HalStorage.h>

#include <cstring>
#include <span>
#include <string>

namespace serialization {
// Borrowed input with a shared cursor for nested page/block deserializers.
class Input {
 public:
  explicit Input(HalFile& source) : file(&source), remainingBytes(source.size() - source.position()) {}
  explicit Input(std::span<const uint8_t> source) : data(source.data()), remainingBytes(source.size()) {}

  size_t read(void* destination, size_t count) {
    if (failed || count > remainingBytes) {
      failed = true;
      std::memset(destination, 0, count);
      return 0;
    }
    if (file) {
      if (static_cast<size_t>(file->read(destination, count)) != count) {
        failed = true;
        std::memset(destination, 0, count);
        return 0;
      }
    } else if (count) {
      std::memcpy(destination, data, count);
      data += count;
    }
    remainingBytes -= count;
    return count;
  }

  bool good() const { return !failed; }
  size_t remaining() const { return remainingBytes; }
  void fail() { failed = true; }

 private:
  HalFile* file = nullptr;
  const uint8_t* data = nullptr;
  size_t remainingBytes;
  bool failed = false;
};

template <typename T>
void readPod(Input& input, T& value) {
  input.read(&value, sizeof(value));
}

inline void readString(Input& input, std::string& value) {
  uint32_t size = 0;
  readPod(input, size);
  if (!input.good() || size > input.remaining()) {
    input.fail();
    value.clear();
    return;
  }
  value.resize(size);
  input.read(value.data(), size);
}
}  // namespace serialization
