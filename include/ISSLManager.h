#ifndef ISSL_MANAGER_H
#define ISSL_MANAGER_H

#include "ISocketManager.h"
#include <string>

namespace exasol {

/**
 * Interface for SSL/TLS operations (Dependency Inversion Principle)
 */
class ISSLManager {
public:
    virtual ~ISSLManager() = default;
    virtual void initialize(const std::string& ca_cert_path, const std::string& server_name) = 0;
    virtual void load_client_certificate(const std::string& cert_path, const std::string& key_path) = 0;
    virtual void attach_socket(socket_t socket) = 0;
    virtual void handshake() = 0;
    virtual std::string read() = 0;
    virtual int read(char* buffer, int size) = 0;  // Raw read for larger buffers
    virtual void write(const std::string& data) = 0;
    virtual std::string get_cipher() const = 0;
    virtual void shutdown() = 0;
};

} // namespace exasol

#endif // ISSL_MANAGER_H
