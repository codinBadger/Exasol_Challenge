#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "ISocketManager.h"
#include <string>
#include <memory>

namespace exasol {

/**
 * Platform-specific socket management
 * Single Responsibility: Handles TCP socket operations
 */
class SocketManager : public ISocketManager {
public:
    SocketManager();
    ~SocketManager() override;
    
    socket_t connect(const std::string& address, uint16_t port) override;
    void close(socket_t socket) override;
    bool is_valid(socket_t socket) const override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace exasol

#endif // SOCKET_MANAGER_H
