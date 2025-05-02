#pragma once

#include <utility>

#include <liburing.h>

namespace uring {

class Cqe {
public:
  Cqe(io_uring* ring, io_uring_cqe* cqe) : m_ring(ring), m_cqe(cqe) {}

  Cqe(const Cqe&) = delete;
  Cqe(Cqe&& other) : m_ring(std::exchange(other.m_ring, nullptr)), m_cqe(std::exchange(other.m_cqe, nullptr)) {}

  ~Cqe() {
    if (m_cqe != nullptr) {
      io_uring_cqe_seen(m_ring, m_cqe);
    }
  }

  io_uring_cqe* operator->() { return m_cqe; }

  io_uring* ring() { return m_ring; }

private:

  io_uring* m_ring;
  io_uring_cqe* m_cqe;
};

}
