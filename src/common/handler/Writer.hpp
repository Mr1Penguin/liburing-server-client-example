#pragma once

#include <vector>

namespace handler {

class Writer {
public:
  Writer(std::vector<std::byte>&& data) : m_buffer(std::move(data)) {}

private:
  std::vector<std::byte> m_buffer;
};

} // namespace handler
