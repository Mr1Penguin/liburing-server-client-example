#pragma once

#include <vector>

namespace handler {
class Reader {
public:
  Reader(size_t n) : m_buffer(n) {}

protected:
  template<class Self>
  auto&& buffer(this Self&& self) {
    return std::forward<Self>(self).m_buffer;
  }

private:
  std::vector<std::byte> m_buffer;
};

} // namespace handler
