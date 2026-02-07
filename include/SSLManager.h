#ifndef SSL_MANAGER_H
#define SSL_MANAGER_H

#include "ISSLManager.h"
#include <memory>

namespace exasol {

/**
 * OpenSSL-based SSL/TLS management
 * Single Responsibility: Handles SSL/TLS operations
 */
class SSLManager : public ISSLManager {
public:
    SSLManager();
    ~SSLManager() override;
    
    void initialize(const std::string& ca_cert_path, const std::string& server_name) override;
    void load_client_certificate(const std::string& cert_path, const std::string& key_path) override;
    void attach_socket(socket_t socket) override;
    void handshake() override;
    std::string read() override;
    int read(char* buffer, int size) override;  // Raw read
    void write(const std::string& data) override;
    std::string get_cipher() const override;
    void shutdown() override;
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace exasol

#endif // SSL_MANAGER_H
