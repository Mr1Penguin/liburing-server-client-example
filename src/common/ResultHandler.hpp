#pragma once

#include "uring/Cqe.hpp"

class ResultHandler {
public:
  virtual ~ResultHandler() = default;

  virtual void operator()(uring::Cqe cqe) = 0;
};

