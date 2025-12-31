#ifndef FILE_CONFIG_LOADER_H
#define FILE_CONFIG_LOADER_H

#include "IConfigLoader.h"
#include <string>
#include <vector>

namespace exasol {

/**
 * Loads configuration from a file
 * Single Responsibility: Only handles file-based config loading
 */
class FileConfigLoader : public IConfigLoader {
public:
    explicit FileConfigLoader(const std::string& file_path);
    ClientConfig load() override;

private:
    std::string file_path_;
    
    std::string trim(const std::string& s) const;
    uint16_t parse_port(const std::string& text) const;
    std::vector<uint16_t> parse_ports(const std::string& text) const;
};

} // namespace exasol

#endif // FILE_CONFIG_LOADER_H
