#include "../include/SocketManager.h"
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace exasol {

#ifdef _WIN32
const socket_t kInvalidSocket = INVALID_SOCKET;
#else
const socket_t kInvalidSocket = -1;
#endif

class SocketManager::Impl {
public:
    Impl() {
        platform_startup();
    }
    
    ~Impl() {
        platform_cleanup();
    }
    
    socket_t connect(const std::string& address, uint16_t port) {
        socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == kInvalidSocket) {
            throw std::runtime_error("socket() failed");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            close(sock);
            throw std::runtime_error("inet_pton() failed - check IP address");
        }

        if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(sock);
            throw std::runtime_error("connect() failed");
        }

        return sock;
    }
    
    void close(socket_t s) {
#ifdef _WIN32
        closesocket(s);
#else
        ::close(s);
#endif
    }
    
    bool is_valid(socket_t s) const {
        return s != kInvalidSocket;
    }

private:
    void platform_startup() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }
    
    void platform_cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

SocketManager::SocketManager() : pImpl_(std::make_unique<Impl>()) {}

SocketManager::~SocketManager() = default;

socket_t SocketManager::connect(const std::string& address, uint16_t port) {
    return pImpl_->connect(address, port);
}

void SocketManager::close(socket_t socket) {
    pImpl_->close(socket);
}

bool SocketManager::is_valid(socket_t socket) const {
    return pImpl_->is_valid(socket);
}

} // namespace exasol
