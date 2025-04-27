#pragma once

#include "UringCqe.hpp"

class ResultHandler {
public:
  virtual ~ResultHandler() = default;

  virtual void operator()(UringCqe cqe) = 0;
};
