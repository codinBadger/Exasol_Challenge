#include "../include/CLIConfigLoader.h"

namespace exasol {

CLIConfigLoader::CLIConfigLoader(const std::string& address, uint16_t port, const std::string& ca_cert) {
    config_.address = address;
    config_.port = port;
    config_.ca_cert = ca_cert;
    config_.ports = {port};
}

ClientConfig CLIConfigLoader::load() {
    return config_;
}

} // namespace exasol
