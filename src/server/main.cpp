#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <stop_token>
#include <unistd.h>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>

#include <liburing.h>

#include "socket.hpp"

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
  io_uring_queue_init(10, &ring, 0);

  auto stopToken = stopSource.get_token();

  io_uring_cqe* cqe{};
  enum class Event : uint32_t { accept = 1, readOdd, readEven, timeout };
  struct Result {
    Event event;
    int fd;
  };

  auto acceptReq = [&ring, socketDescriptor] {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, socketDescriptor, nullptr, nullptr, 0);
    auto* res = new Result{.event = Event::accept, .fd = 0};
    io_uring_sqe_set_data(sqe, res);
    io_uring_submit(&ring);
  };

  struct Client {
    std::vector<char> buffer;
    int receivedOdd{0};
    int receivedEven{0};
  };

  std::map<int32_t, Client> clients{};

  auto readOddReq = [&ring, &clients](int client) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    clients[client].buffer.resize(4096);
    sqe->flags |= IOSQE_IO_LINK;
    io_uring_prep_read(sqe, client, clients[client].buffer.data(), 5, 0);
    auto* res = new Result{.event = Event::readOdd, .fd = client};
    io_uring_sqe_set_data(sqe, res);
    io_uring_submit(&ring);
  };

  auto readEvenReq = [&ring, &clients](int client) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    clients[client].buffer.resize(4096);
    sqe->flags |= IOSQE_IO_LINK;
    io_uring_prep_read(sqe, client, clients[client].buffer.data(), 5, 0);
    auto* res = new Result{.event = Event::readEven, .fd = client};
    io_uring_sqe_set_data(sqe, res);
    sqe = io_uring_get_sqe(&ring);
    __kernel_timespec ts{.tv_sec = 0, .tv_nsec = std::chrono::nanoseconds(std::chrono::milliseconds(500)).count()};
    io_uring_prep_link_timeout(sqe, &ts, 0);
    res = new Result{.event = Event::timeout, .fd = client};
    io_uring_sqe_set_data(sqe, res);
    io_uring_submit(&ring);
  };

  acceptReq();

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

    auto result = std::unique_ptr<Result>(reinterpret_cast<Result*>(cqe->user_data));
    // std::clog << "Received res with event " << static_cast<uint32_t>(result->event) << '\n';
    switch (result->event) {
    case Event::accept:
      acceptReq();
      clients.emplace(cqe->res, Client{});
      readOddReq(cqe->res);
      std::cout << "new client: " << cqe->res << '\n';
      break;

    case Event::readOdd:
      if (cqe->res < 0) {
        std::cerr << "readOdd error: " << strerror(-cqe->res);
      } else if (cqe->res == 0) {
        std::clog << "odd: client " << result->fd << " closed connection\n";
        close(result->fd);
        std::clog << "client " << result->fd << " stats: " << clients[result->fd].receivedOdd << ';'
                  << clients[result->fd].receivedEven << '\n';
        clients.erase(result->fd);
      } else {
        if (clients[result->fd].buffer[0] == '1' && clients[result->fd].buffer[1] == '3' &&
            clients[result->fd].buffer[2] == '5' && clients[result->fd].buffer[3] == '7' &&
            clients[result->fd].buffer[4] == '9') {
          clients[result->fd].receivedOdd++;
          std::fill_n(std::begin(clients[result->fd].buffer), char(0), 5);
        }
        readEvenReq(result->fd);
      }

      break;

    case Event::readEven:
      if (cqe->res == -ECANCELED) {
        std::clog << "readEven " << result->fd << " timedout\n";
      } else if (cqe->res < 0) {
        std::cerr << "readEven error: " << strerror(-cqe->res);
      } else if (cqe->res == 0) {
        std::clog << "even: client " << result->fd << " closed connection\n";
        close(result->fd);
        std::clog << "client " << result->fd << " stats: " << clients[result->fd].receivedOdd << ';'
                  << clients[result->fd].receivedEven << '\n';
        clients.erase(result->fd);
      } else {
        if (clients[result->fd].buffer[0] == '2' && clients[result->fd].buffer[1] == '4' &&
            clients[result->fd].buffer[2] == '6' && clients[result->fd].buffer[3] == '8' &&
            clients[result->fd].buffer[4] == '0') {
          clients[result->fd].receivedEven++;
          std::fill_n(std::begin(clients[result->fd].buffer), char(0), 5);
        }
        readOddReq(result->fd);
      }

      break;

    case Event::timeout:
      break;

    default:
      std::clog << "default\n";
      stopSource.request_stop();
      break;
    }

    io_uring_cqe_seen(&ring, cqe);
  }

  close(socketDescriptor);

  std::clog << "Bye\n";

  return 0;
}
