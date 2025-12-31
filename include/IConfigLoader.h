#ifndef ICONFIG_LOADER_H
#define ICONFIG_LOADER_H

#include <string>
#include <cstdint>
#include <vector>

namespace exasol {

/**
 * Configuration data structure
 */
struct ClientConfig {
    std::string address;
    uint16_t port{};
    std::string ca_cert;
    std::vector<uint16_t> ports; // optional list of ports to try, first entry used when present
    std::string client_cert;      // optional client certificate (PEM chain) for mTLS
    std::string client_key;       // optional client private key for mTLS
    std::string server_name;      // optional SNI/hostname for TLS verification
};

/**
 * Interface for configuration loading (Dependency Inversion Principle)
 */
class IConfigLoader {
public:
    virtual ~IConfigLoader() = default;
    virtual ClientConfig load() = 0;
};

} // namespace exasol

#endif // ICONFIG_LOADER_H
