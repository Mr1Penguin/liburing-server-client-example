#include "HeaderReader.hpp"

#include "Message.hpp"

namespace handler {

HeaderReader::HeaderReader() : Reader(sizeof(message::Header)) {}

void HeaderReader::operator()(uring::Cqe cqe) {
  if (cqe->res < 0) {
    // error
    return;
  }

  if (cqe->res == 0) {
    // closed
    return;
  }
  
  const auto* header = reinterpret_cast<const message::Header*>(std::data(buffer()));

  switch (header->type) {
  case message::Header::Type::Noop:
    break;
  case message::Header::Type::ReadEntry:
    break;
  }

  return;
}

}
