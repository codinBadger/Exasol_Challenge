#include "../include/FileConfigLoader.h"
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace exasol {

FileConfigLoader::FileConfigLoader(const std::string& file_path)
    : file_path_(file_path) {}

ClientConfig FileConfigLoader::load() {
    std::ifstream in(file_path_);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open config file: " + file_path_);
    }

    std::string line, address, ca_cert, port_text, client_cert, client_key, server_name;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        
        if (key == "server_address") address = value;
        else if (key == "port") port_text = value;
        else if (key == "ca_cert") ca_cert = value;
        else if (key == "client_cert") client_cert = value;
        else if (key == "client_key") client_key = value;
        else if (key == "server_name") server_name = value;
    }

    if (address.empty() || port_text.empty() || ca_cert.empty()) {
        throw std::runtime_error("Config missing required fields: server_address, port, ca_cert");
    }

    ClientConfig cfg;
    cfg.address = address;
    cfg.ports = parse_ports(port_text);
    if (!cfg.ports.empty()) {
        cfg.port = cfg.ports.front();
    }
    cfg.ca_cert = ca_cert;
    cfg.client_cert = client_cert;
    cfg.client_key = client_key;
    cfg.server_name = server_name;
    return cfg;
}

std::string FileConfigLoader::trim(const std::string& s) const {
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

uint16_t FileConfigLoader::parse_port(const std::string& text) const {
    int value = std::stoi(text);
    if (value < 0 || value > 65535) {
        throw std::runtime_error("Port must be between 0 and 65535");
    }
    return static_cast<uint16_t>(value);
}

std::vector<uint16_t> FileConfigLoader::parse_ports(const std::string& text) const {
    std::vector<uint16_t> result;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        result.push_back(parse_port(token));
    }
    if (result.empty()) {
        throw std::runtime_error("Port must not be empty");
    }
    return result;
}

} // namespace exasol
