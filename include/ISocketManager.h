#ifndef ISOCKET_MANAGER_H
#define ISOCKET_MANAGER_H

#include <string>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace exasol {

#ifdef _WIN32
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

/**
 * Interface for socket operations (Dependency Inversion Principle)
 */
class ISocketManager {
public:
    virtual ~ISocketManager() = default;
    virtual socket_t connect(const std::string& address, uint16_t port) = 0;
    virtual void close(socket_t socket) = 0;
    virtual bool is_valid(socket_t socket) const = 0;
};

} // namespace exasol

#endif // ISOCKET_MANAGER_H
