#include <chrono>
#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <sys/un.h>

#include "socket.hpp"

int main(int argc, char** argv) {
  std::array<char, 256> errnoText{};

  int socketDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);
  sockaddr_un addr{.sun_family = AF_UNIX, .sun_path = {0}};
  strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

  if (connect(socketDescriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
    strerror_r(errno, errnoText.data(), errnoText.size());
    std::cerr << "connect failed: " << errnoText.data() << '\n';
    return 1;
  }

  if (argc == 1) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  } else {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    switch (argv[1][0]) {
    case '1':
      for (auto i = 0; i < 1000; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        write(socketDescriptor, "13579", 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        write(socketDescriptor, "24680", 5);
      }
      break;
    case '2':
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      write(socketDescriptor, "13579", 5);
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      write(socketDescriptor, "24680", 5);
      break;
    case '3':
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      write(socketDescriptor, "13579", 5);
      std::this_thread::sleep_for(std::chrono::milliseconds(2500));
      write(socketDescriptor, "24680", 5);
      break;
    }
  }

  close(socketDescriptor);

  return 0;
}
