#pragma once

#include "Reader.hpp"
#include "ResultHandler.hpp"

namespace handler {

class HeaderReader : public Reader, public ResultHandler {
public:
  HeaderReader();
  void operator()(uring::Cqe cqe) override;
};
}
