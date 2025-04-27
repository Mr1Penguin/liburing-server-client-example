#pragma once

#include <utility>

#include <liburing.h>

class UringCqe {
public:
  UringCqe(io_uring* ring, io_uring_cqe* cqe) : ring(ring), cqe(cqe) {}

  UringCqe(const UringCqe&) = delete;
  UringCqe(UringCqe&& other) : ring(std::exchange(other.ring, nullptr)), cqe(std::exchange(other.cqe, nullptr)) {}

  ~UringCqe() {
    if (cqe != nullptr) {
      io_uring_cqe_seen(ring, cqe);
    }
  }

  io_uring* ring;
  io_uring_cqe* cqe;
};
