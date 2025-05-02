#pragma once

#include <cstdint>

namespace message {

using StringSize = uint16_t;

struct Header {
  enum class Type : uint8_t { Noop, ReadEntry };

  uint16_t payloadSize;
  Type type;
};

struct Noop {};

struct ReadEntry {
  StringSize namespaceNameSize;
  StringSize entryNameSize;
};
}
