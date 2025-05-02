#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stop_token>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <liburing.h>

#include "socket.hpp"

#include "ResultHandler.hpp"
#include "handler/Reader.hpp"

void acceptReq(io_uring& ring, int socketDescriptor);
void readEvenReq(io_uring& ring, int fd);
void readOddReq(io_uring& ring, int fd);

bool operator==(std::byte left, char right) { return left == static_cast<std::byte>(right); }

class TestOddReader : public handler::Reader, public ResultHandler {
public:
  TestOddReader(int fd, int size) : Reader(size), m_fd(fd) {}
  void operator()(uring::Cqe cqe) override {
    if (cqe->res < 0) {
      std::cerr << "readOdd error: " << strerror(-cqe->res) << '\n';
    } else if (cqe->res == 0) {
      std::clog << "odd: client " << m_fd << " closed connection\n";
      close(m_fd);
      return;
    } else {
      if (!(buffer()[0] == '1' && buffer()[1] == '3' && buffer()[2] == '5' && buffer()[3] == '7' &&
            buffer()[4] == '9')) {
        std::cerr << "unexpected data from " << m_fd << '\n';
      }
    }

    readEvenReq(*cqe.ring(), m_fd);
  }

  std::byte* data() { return buffer().data(); }

private:
  int m_fd;
};

class TestEvenReader : public handler::Reader, public ResultHandler {
public:
  TestEvenReader(int fd, int size) : Reader(size), m_fd(fd) {}
  void operator()(uring::Cqe cqe) override {
    if (cqe->res < 0) {
      std::cerr << "readOdd error: " << strerror(-cqe->res) << '\n';
    } else if (cqe->res == 0) {
      std::clog << "even: client " << m_fd << " closed connection\n";
      close(m_fd);
      return;
    } else {
      if (!(buffer()[0] == '2' && buffer()[1] == '4' && buffer()[2] == '6' && buffer()[3] == '8' &&
            buffer()[4] == '0')) {
        std::cerr << "unexpected data from " << m_fd << '\n';
      }
    }

    readOddReq(*cqe.ring(), m_fd);
  }

  std::byte* data() { return buffer().data(); }

private:
  int m_fd;
};

class NoopHandler : public ResultHandler {
public:
  void operator()(uring::Cqe /*cqe*/) override { };
};

class AcceptHandler : public ResultHandler {
public:
  AcceptHandler(int socketDescriptor) : m_socketDescriptor(socketDescriptor) {}

  void operator()(uring::Cqe cqe) override {
    std::cout << "new client: " << cqe->res << '\n';
    acceptReq(*cqe.ring(), m_socketDescriptor);
    readOddReq(*cqe.ring(), cqe->res);
  };

private:
  int m_socketDescriptor;
};

void acceptReq(io_uring& ring, int socketDescriptor) {
  io_uring_sqe* sqe = io_uring_get_sqe(&ring);
  io_uring_prep_accept(sqe, socketDescriptor, nullptr, nullptr, 0);
  auto* res = new AcceptHandler(socketDescriptor);
  io_uring_sqe_set_data(sqe, res);
  io_uring_submit(&ring);
}

void readOddReq(io_uring& ring, int fd) {
  io_uring_sqe* sqe = io_uring_get_sqe(&ring);
  auto* reader      = new TestOddReader(fd, 5);
  io_uring_prep_read(sqe, fd, reader->data(), 5, 0);
  io_uring_sqe_set_data(sqe, reader);
  io_uring_submit(&ring);
}

void readEvenReq(io_uring& ring, int fd) {
  io_uring_sqe* sqe = io_uring_get_sqe(&ring);
  sqe->flags |= IOSQE_IO_LINK;
  auto* reader = new TestEvenReader{fd, 5};
  io_uring_prep_read(sqe, fd, reader->data(), 5, 0);
  io_uring_sqe_set_data(sqe, reader);
  sqe = io_uring_get_sqe(&ring);
  __kernel_timespec ts{.tv_sec = 0, .tv_nsec = std::chrono::nanoseconds(std::chrono::milliseconds(500)).count()};
  auto* noop = new NoopHandler{};
  io_uring_prep_link_timeout(sqe, &ts, 0);
  io_uring_sqe_set_data(sqe, noop);
  io_uring_submit(&ring);
}

std::stop_source stopSource{};

void handleInterrupt(int /*signal*/) { stopSource.request_stop(); }

int main(int /*argc*/, char** /*argv*/) {
  std::array<char, 256> errnoText{};

  int socketDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);

  if (unlink(socketPath) != 0 && errno != ENOENT) {
    strerror_r(errno, errnoText.data(), errnoText.size());
    std::cerr << "unlink failed: " << errnoText.data() << '\n';
    return 1;
  }

  sockaddr_un addr{.sun_family = AF_UNIX, .sun_path = {0}};
  strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

  if (bind(socketDescriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
    strerror_r(errno, errnoText.data(), errnoText.size());
    std::cerr << "bind failed: " << errnoText.data() << '\n';
    return 1;
  }

  if (listen(socketDescriptor, 10) == -1) {
    strerror_r(errno, errnoText.data(), errnoText.size());
    std::cerr << "listen failed: " << errnoText.data() << '\n';
    return 1;
  }

  io_uring ring;

  signal(SIGINT, handleInterrupt);
  io_uring_queue_init(10, &ring, IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN);

  auto stopToken = stopSource.get_token();

  io_uring_cqe* cqe{};
  acceptReq(ring, socketDescriptor);

  while (!stopToken.stop_requested()) {
    const int waitRes = io_uring_wait_cqe(&ring, &cqe);
    if (waitRes < 0) {
      if (-waitRes == EINTR) {
        continue;
      }

      strerror_r(errno, errnoText.data(), errnoText.size());
      std::cerr << "wait failed: " << errnoText.data() << '\n';
      return 1;
    }

    auto resultHandler = std::unique_ptr<ResultHandler>(reinterpret_cast<ResultHandler*>(cqe->user_data));
    (*resultHandler)(uring::Cqe(&ring, cqe));
  }

  close(socketDescriptor);

  std::clog << "Bye\n";

  return 0;
}
