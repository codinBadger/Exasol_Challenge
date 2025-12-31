#ifndef CLI_CONFIG_LOADER_H
#define CLI_CONFIG_LOADER_H

#include "IConfigLoader.h"
#include <string>

namespace exasol {

/**
 * Loads configuration from command-line arguments
 * Single Responsibility: Only handles CLI-based config loading
 */
class CLIConfigLoader : public IConfigLoader {
public:
    CLIConfigLoader(const std::string& address, uint16_t port, const std::string& ca_cert);
    ClientConfig load() override;

private:
    ClientConfig config_;
};

} // namespace exasol

#endif // CLI_CONFIG_LOADER_H
