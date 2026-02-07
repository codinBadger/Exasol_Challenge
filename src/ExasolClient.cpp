#include "../include/ExasolClient.h"
#include <iostream>
#include <stdexcept>
#include <cstring> // for memset
#include <vector>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ExasolClient.h"
#include "../include/FileConfigLoader.h"
#include "../include/CLIConfigLoader.h"
#include "../include/SocketManager.h"
#include "../include/SSLManager.h"
#include <fstream>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace exasol
{

    ExasolClient::ExasolClient(
        std::unique_ptr<IConfigLoader> config_loader,
        std::unique_ptr<ISocketManager> socket_manager,
        std::unique_ptr<ISSLManager> ssl_manager) : config_loader_(std::move(config_loader)),
                                                    socket_manager_(std::move(socket_manager)),
                                                    ssl_manager_(std::move(ssl_manager)),
                                                    socket_(0),
                                                    connected_(false) {}

    ExasolClient::~ExasolClient()
    {
        if (connected_)
        {
            disconnect();
        }
    }

// Minimal, bare-metal SSLManager implementation in this compilation unit
// This replaces the separate src/SSLManager.cpp to keep essentials in one file.
class SSLManager::Impl : public ISSLManager {
public:
    Impl() : ctx_(nullptr), ssl_(nullptr) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
    }

    ~Impl() override {
        if (ssl_) SSL_free(ssl_);
        if (ctx_) SSL_CTX_free(ctx_);
        EVP_cleanup();
    }

    void initialize(const std::string& ca_cert_path, const std::string& server_name) override {
        const SSL_METHOD* method = TLS_client_method();
        ctx_ = SSL_CTX_new(method);
        if (!ctx_) throw std::runtime_error("Unable to create SSL context");

        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
        if (!ca_cert_path.empty()) {
            if (SSL_CTX_load_verify_locations(ctx_, ca_cert_path.c_str(), nullptr) != 1) {
                throw std::runtime_error("Failed to load CA certificate for verification");
            }
        }
        server_name_ = server_name;
    }

    void load_client_certificate(const std::string& cert_path, const std::string& key_path) override {
        if (cert_path.empty() || key_path.empty()) return;
        if (!ctx_) throw std::runtime_error("SSL context not initialized");
        if (SSL_CTX_use_certificate_chain_file(ctx_, cert_path.c_str()) != 1)
            throw std::runtime_error("Failed to load client certificate chain");
        if (SSL_CTX_use_PrivateKey_file(ctx_, key_path.c_str(), SSL_FILETYPE_PEM) != 1)
            throw std::runtime_error("Failed to load client private key");
        if (SSL_CTX_check_private_key(ctx_) != 1) throw std::runtime_error("Client private key does not match certificate");
    }

    void attach_socket(socket_t socket) override {
        if (!ctx_) throw std::runtime_error("SSL context not initialized");
        if (ssl_) { SSL_free(ssl_); ssl_ = nullptr; }
        ssl_ = SSL_new(ctx_);
        if (!ssl_) throw std::runtime_error("Failed to create SSL structure");
        SSL_set_fd(ssl_, static_cast<int>(socket));
        if (!server_name_.empty()) SSL_set_tlsext_host_name(ssl_, server_name_.c_str());
    }

    void handshake() override {
        if (!ssl_) throw std::runtime_error("SSL not attached to socket");
        if (SSL_connect(ssl_) <= 0) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("TLS handshake failed");
        }
    }

    std::string read() override {
        if (!ssl_) throw std::runtime_error("SSL not initialized");
        char buffer[4096]{};
        int bytes = SSL_read(ssl_, buffer, sizeof(buffer) - 1);
        if (bytes > 0) { buffer[bytes] = '\0'; return std::string(buffer); }
        return std::string();
    }

    int read(char* buffer, int size) override {
        if (!ssl_) throw std::runtime_error("SSL not initialized");
        return SSL_read(ssl_, buffer, size);
    }

    void write(const std::string& data) override {
        if (!ssl_) throw std::runtime_error("SSL not initialized");
        SSL_write(ssl_, data.c_str(), static_cast<int>(data.length()));
    }

    std::string get_cipher() const override {
        if (!ssl_) return std::string("Not connected");
        const char* cipher = SSL_get_cipher(ssl_);
        return cipher ? cipher : std::string("Unknown");
    }

    void shutdown() override {
        if (ssl_) SSL_shutdown(ssl_);
    }

private:
    SSL_CTX* ctx_;
    SSL* ssl_;
    std::string server_name_;
};

SSLManager::SSLManager() : pImpl_(std::make_unique<Impl>()) {}
SSLManager::~SSLManager() = default;

void SSLManager::initialize(const std::string& ca_cert_path, const std::string& server_name) { pImpl_->initialize(ca_cert_path, server_name); }
void SSLManager::load_client_certificate(const std::string& cert_path, const std::string& key_path) { pImpl_->load_client_certificate(cert_path, key_path); }
void SSLManager::attach_socket(socket_t socket) { pImpl_->attach_socket(socket); }
void SSLManager::handshake() { pImpl_->handshake(); }
std::string SSLManager::read() { return pImpl_->read(); }
int SSLManager::read(char* buffer, int size) { return pImpl_->read(buffer, size); }
void SSLManager::write(const std::string& data) { pImpl_->write(data); }
std::string SSLManager::get_cipher() const { return pImpl_->get_cipher(); }
void SSLManager::shutdown() { pImpl_->shutdown(); }

// Minimal FileConfigLoader implementation (moved here to keep essentials in one file)
FileConfigLoader::FileConfigLoader(const std::string& file_path)
    : file_path_(file_path) {}

ClientConfig FileConfigLoader::load() {
    std::ifstream in(file_path_);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open config file: " + file_path_);
    }

    std::string line, address, ca_cert, port_text, client_cert, client_key, server_name;
    while (std::getline(in, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        auto key = line.substr(0, pos);
        auto value = line.substr(pos + 1);
        // trim
        auto trim = [](std::string &s){
            const char* ws = " \t\r\n";
            size_t a = s.find_first_not_of(ws);
            if (a==std::string::npos) { s.clear(); return; }
            size_t b = s.find_last_not_of(ws);
            s = s.substr(a, b-a+1);
        };
        trim(key); trim(value);
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
    // parse ports (comma separated)
    std::stringstream ss(port_text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // trim token
        const char* ws = " \t\r\n";
        size_t a = token.find_first_not_of(ws);
        if (a==std::string::npos) continue;
        size_t b = token.find_last_not_of(ws);
        token = token.substr(a, b-a+1);
        int p = std::stoi(token);
        cfg.ports.push_back(static_cast<uint16_t>(p));
    }
    if (!cfg.ports.empty()) cfg.port = cfg.ports.front();
    cfg.ca_cert = ca_cert;
    cfg.client_cert = client_cert;
    cfg.client_key = client_key;
    cfg.server_name = server_name;
    return cfg;
}

// Minimal CLIConfigLoader implementation
CLIConfigLoader::CLIConfigLoader(const std::string& address, uint16_t port, const std::string& ca_cert) {
    config_.address = address;
    config_.port = port;
    config_.ca_cert = ca_cert;
}

ClientConfig CLIConfigLoader::load() {
    return config_;
}

// Minimal SocketManager implementation
#ifdef _WIN32
const socket_t kInvalidSocket = INVALID_SOCKET;
#else
const socket_t kInvalidSocket = -1;
#endif

SocketManager::SocketManager() {}
SocketManager::~SocketManager() = default;

socket_t SocketManager::connect(const std::string& address, uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
    socket_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kInvalidSocket) throw std::runtime_error("socket() failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        #ifdef _WIN32
        closesocket(sock);
        #else
        ::close(sock);
        #endif
        throw std::runtime_error("inet_pton() failed - check IP address");
    }
    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        #ifdef _WIN32
        closesocket(sock);
        #else
        ::close(sock);
        #endif
        throw std::runtime_error("connect() failed");
    }
    return sock;
}

void SocketManager::close(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

bool SocketManager::is_valid(socket_t s) const {
    return s != kInvalidSocket;
}



    std::string ExasolClient::sha1_hex(const std::string &input)
    {
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

        std::ostringstream oss;
        for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    void ExasolClient::connect()
    {
        try
        {
            // Load configuration
            config_ = config_loader_->load();

            // Normalize ports list (support legacy single-port configs)
            if (config_.ports.empty() && config_.port != 0)
            {
                config_.ports.push_back(config_.port);
            }
            if (config_.ports.empty())
            {
                throw std::runtime_error("No ports provided in configuration");
            }

            const int max_attempts = 10;
            const auto retry_delay = std::chrono::seconds(3);

            // Initialize SSL context once; SSL objects reset per attempt in attach_socket
            ssl_manager_->initialize(config_.ca_cert, config_.server_name);
            if (!config_.client_cert.empty() && !config_.client_key.empty())
            {
                ssl_manager_->load_client_certificate(config_.client_cert, config_.client_key);
            }

            for (int attempt = 1; attempt <= max_attempts; ++attempt)
            {
                uint16_t port_to_try = config_.ports[(attempt - 1) % config_.ports.size()];
                try
                {
                    // Create socket connection
                    socket_ = socket_manager_->connect(config_.address, port_to_try);

                    // Attach socket to SSL and perform handshake
                    ssl_manager_->attach_socket(socket_);
                    ssl_manager_->handshake();

                    connected_ = true;
                    std::cout << "Connected to server with cipher: " << get_cipher()
                              << " on port " << port_to_try
                              << " (attempt " << attempt << ")\n";
                    return;
                }
                catch (const std::exception &ex)
                {
                    if (socket_manager_->is_valid(socket_))
                    {
                        socket_manager_->close(socket_);
                    }
                    socket_ = 0;

                    if (attempt == max_attempts)
                    {
                        std::cerr << "Connection failed after " << max_attempts
                                  << " attempts. Last error: " << ex.what() << "\n";
                        throw;
                    }

                    std::cout << "Connect attempt " << attempt << " failed on port "
                              << port_to_try << ": " << ex.what()
                              << " — retrying in " << retry_delay.count() << "s\n";
                    std::this_thread::sleep_for(retry_delay);
                }
            }
        }
        catch (const std::exception &ex)
        {
            if (socket_manager_->is_valid(socket_))
            {
                socket_manager_->close(socket_);
            }
            throw;
        }
    }

    // Optimized: Generate suffix using hex encoding (deterministic counter)
    std::string generate_suffix(uint64_t counter)
    {
        std::ostringstream oss;
        oss << std::hex << counter;
        return oss.str();
    }

    // Random hex suffix (8 chars by default)
    std::string random_hex_string(size_t length = 8)
    {
        thread_local std::mt19937_64 gen{std::random_device{}()};
        std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);
        std::ostringstream oss;
        oss << std::hex << dis(gen);
        std::string result = oss.str();
        if (result.length() > length)
        {
            result = result.substr(0, length);
        }
        return result;
    }

    std::string ExasolClient::random_string(size_t length = 16)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        // Allowed characters: printable ASCII except space, tab, CR, LF
        std::string allowed;
        for (int c = 33; c <= 126; ++c)
        {
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            {
                allowed += static_cast<char>(c);
            }
        }

        std::uniform_int_distribution<> dis(0, allowed.length() - 1);
        std::string result;
        for (size_t i = 0; i < length; ++i)
        {
            result += allowed[dis(gen)];
        }
        return result;
    }
inline void sha1_raw(const unsigned char* data, size_t len,
                     unsigned char out[SHA_DIGEST_LENGTH]) {
    SHA1(data, len, out);
}

inline bool check_difficulty(const unsigned char hash[SHA_DIGEST_LENGTH], int difficulty) {
    int full_bytes = difficulty / 2;
    for (int i = 0; i < full_bytes; ++i)
        if (hash[i] != 0) return false;

    if (difficulty & 1)
        return (hash[full_bytes] & 0xF0) == 0;

    return true;
}

// Optimized POW solver using SHA1 incremental API
struct SHA1Precomputed {
    SHA_CTX base_ctx;
    size_t auth_len;
    
    void init(const std::string& authdata) {
        auth_len = authdata.size();
        SHA1_Init(&base_ctx);
        SHA1_Update(&base_ctx, authdata.data(), auth_len);
    }
    
    inline void hash_counter(uint64_t counter, unsigned char out[SHA_DIGEST_LENGTH]) {
        SHA_CTX ctx = base_ctx;  // Copy pre-computed state
        SHA1_Update(&ctx, &counter, sizeof(counter));
        SHA1_Final(out, &ctx);
    }
};

    void ExasolClient::communicate()
    {
        if (!connected_)
        {
            throw std::runtime_error("Not connected to server");
        }

        try
        {
            char buffer[4096]{};
            std::string authdata;
            bool authenticated = false;
            while (true)
            {
                int bytes_received = ssl_manager_->read(buffer, sizeof(buffer) - 1);

                if (bytes_received <= 0)
                {
                    std::cout << "Connection closed or error occurred." << std::endl;
                    break;
                }

                buffer[bytes_received] = '\0';
                std::string data(buffer, bytes_received);

                // Trim trailing whitespace
                size_t end = data.find_last_not_of(" \t\r\n");
                if (end != std::string::npos)
                {
                    data.resize(end + 1);
                }

                std::cout << "Server command: " << data << "\n";

                // Split by space to get args
                std::vector<std::string> args;
                std::istringstream iss(data);
                std::string token;
                while (iss >> token)
                {
                    args.push_back(token);
                }

                if (args.empty())
                {
                    std::cout << "Empty command received\n";
                    continue;
                }

                // Prepare response based on command
                std::string response;
                if (args[0] == "HELO")
                {
                    response = "EHLO\n";
                    std::cout << "Responding with: EHLO\n";
                }
                else if (args[0] == "ERROR")
                {
                    std::cout << "ERROR: ";
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        std::cout << args[i];
                        if (i < args.size() - 1)
                            std::cout << " ";
                    }
                    std::cout << "\n";
                    break;
                }
                else if (args[0] == "POW")
                {
                    if (args.size() >= 3)
                    {
                        authdata = args[1];
                        int difficulty = std::stoi(args[2]);

                        std::atomic<bool> found(false);
                        std::mutex result_mutex;
                        std::string found_suffix;

                        const unsigned int num_threads =
                            std::max(1u, std::thread::hardware_concurrency());

                        auto pow_worker = [&](unsigned int tid)
                        {
                            unsigned char hash[SHA_DIGEST_LENGTH];
                            uint64_t counter = tid;
                            const uint64_t stride = num_threads;
                            constexpr int CHECK_INTERVAL = 4096;  // Check found flag less often

                            while (true)
                            {
                                for (int batch = 0; batch < CHECK_INTERVAL; ++batch)
                                {
                                    // Combine authdata with counter
                                    std::string data_to_hash = authdata + std::to_string(counter);
                                    sha1_raw(reinterpret_cast<const unsigned char*>(data_to_hash.c_str()), 
                                             data_to_hash.length(), hash);

                                    if (check_difficulty(hash, difficulty))
                                    {
                                        std::lock_guard<std::mutex> lock(result_mutex);
                                        if (!found)
                                        {
                                            found = true;
                                            found_suffix = std::to_string(counter);
                                            std::cout << "[Thread " << tid
                                                      << "] POW solved: " << found_suffix << "\n";
                                        }
                                        return;
                                    }
                                    counter += stride;
                                }
                                if (found.load(std::memory_order_relaxed)) return;
                            }
                        };

                        std::vector<std::thread> threads;
                        for (unsigned int i = 0; i < num_threads; ++i)
                            threads.emplace_back(pow_worker, i);

                        for (auto &t : threads)
                            t.join();

                        response = found_suffix + "\n";
                        authenticated = true;
                    }
                    else
                    {
                        response = "POW_ERROR: Insufficient arguments\n";
                    }
                }
                else if (args[0] == "POW2")
                {
                    if (args.size() >= 3)
                    {
                        authdata = args[1];
                        int difficulty = std::stoi(args[2]);

                        std::atomic<bool> found(false);
                        std::mutex result_mutex;
                        std::string found_suffix;

                        const unsigned int num_threads =
                            std::max(1u, std::thread::hardware_concurrency());

                        // Precompute SHA1 state for authdata (shared across threads)
                        SHA1Precomputed precomputed;
                        precomputed.init(authdata);

                        auto pow_worker = [&](unsigned int tid)
                        {
                            unsigned char hash[SHA_DIGEST_LENGTH];
                            uint64_t counter = tid;
                            const uint64_t stride = num_threads;
                            constexpr int CHECK_INTERVAL = 4096;  // Check found flag less often

                            while (true)
                            {
                                for (int batch = 0; batch < CHECK_INTERVAL; ++batch)
                                {
                                    precomputed.hash_counter(counter, hash);

                                    if (check_difficulty(hash, difficulty))
                                    {
                                        std::lock_guard<std::mutex> lock(result_mutex);
                                        if (!found)
                                        {
                                            found = true;
                                            found_suffix = std::to_string(counter);
                                            std::cout << "[Thread " << tid
                                                      << "] POW2 solved: " << found_suffix << "\n";
                                        }
                                        return;
                                    }
                                    counter += stride;
                                }
                                if (found.load(std::memory_order_relaxed)) return;
                            }
                        };

                        std::vector<std::thread> threads;
                        for (unsigned int i = 0; i < num_threads; ++i)
                            threads.emplace_back(pow_worker, i);

                        for (auto &t : threads)
                            t.join();

                        response = found_suffix + "\n";
                        authenticated = true;
                    }
                    else
                    {
                        response = "POW2_ERROR: Insufficient arguments\n";
                    }
                }
                else if (args[0] == "END")
                {
                    // Data submission complete
                    response = "OK\n";
                    std::cout << "Received END command, finishing communication.\n";
                }
                else if (args[0] == "NAME")
                {
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string name_hash = sha1_hex(authdata + challenge);
                        response = name_hash + " " + "Deepak Shivanandham\n";
                        std::cout << "Responding to NAME\n";
                    }
                    else
                    {
                        response = "ERROR: NAME requires authentication\n";
                    }
                }
                else if (args[0] == "MAILNUM")
                {
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string mailnum_hash = sha1_hex(authdata + challenge);
                        response = mailnum_hash + " " + "1\n";
                        std::cout << "Responding to MAILNUM\n";
                    }
                    else
                    {
                        response = "ERROR: MAILNUM requires authentication\n";
                    }
                }
                else if (args[0] == "MAIL1")
                {
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "deepakshivanandham@hotmail.com\n";
                        std::cout << "Responding to MAIL1\n";
                    }
                    else
                    {
                        response = "ERROR: MAIL1 requires authentication\n";
                    }
                }
                else if (args[0] == "SKYPE")
                {
                    // Specify Skype account for the interview, or N/A if no account
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "NA\n";
                    }
                    else
                    {
                        response = "ERROR: SKYPE requires authdata\n";
                    }
                }
                else if (args[0] == "BIRTHDATE")
                {
                    // Specify birthdate in format %d.%m.%Y
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "06.02.1991\n";
                    }
                    else
                    {
                        response = "ERROR: BIRTHDATE requires authdata\n";
                    }
                }
                else if (args[0] == "COUNTRY")
                {
                    // Country where you currently live
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "india\n";
                    }
                    else
                    {
                        response = "ERROR: COUNTRY requires authdata\n";
                    }
                }
                else if (args[0] == "ADDRNUM")
                {
                    // Specifies how many lines your address has
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "2\n";
                    }
                    else
                    {
                        response = "ERROR: ADDRNUM requires authdata\n";
                    }
                }
                else if (args[0] == "ADDRLINE1")
                {
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "25, GAJALAKSHMI NAGAR 1st CROSS STREET\n";
                    }
                    else
                    {
                        response = "ERROR: ADDRLINE1 requires authdata\n";
                    }
                }
                else if (args[0] == "ADDRLINE2")
                {
                    if (authenticated && args.size() >= 2)
                    {
                        std::string challenge = args[1];
                        std::string hash = sha1_hex(authdata + challenge);
                        response = hash + " " + "CHROMPET,CHENNAI, TAMILNADU\n";
                    }
                    else
                    {
                        response = "ERROR: ADDRLINE2 requires authdata\n";
                    }
                }
                else
                {
                    response = "ERROR Unknown command\n";
                }

                // Send response
                if (!response.empty())
                {
                    ssl_manager_->write(response);
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Communication error: " << e.what() << std::endl;
        }
    }

    void ExasolClient::disconnect()
    {
        if (connected_)
        {
            ssl_manager_->shutdown();
            socket_manager_->close(socket_);
            connected_ = false;
            std::cout << "Client disconnected.\n";
        }
    }

    std::string ExasolClient::get_cipher() const
    {
        return ssl_manager_->get_cipher();
    }

    bool ExasolClient::is_connected() const
    {
        return connected_;
    }

    // ============================================================================
    // BENCHMARKING (uses same optimized POW as production code)
    // ============================================================================

    void ExasolClient::benchmark_suffix_generation() {}

    void ExasolClient::benchmark_pow_solving(const std::string &authdata, int difficulty, bool use_multithreading, PowStrategy)
    {
        unsigned int num_threads = use_multithreading ? std::max(1u, std::thread::hardware_concurrency()) : 1;
        std::cout << "POW Benchmark: difficulty=" << difficulty << ", threads=" << num_threads << "\n";

        auto start = std::chrono::high_resolution_clock::now();
        std::atomic<bool> found(false);
        std::atomic<uint64_t> solution(0);
        std::mutex result_mutex;

        auto pow_worker = [&](unsigned int tid) {
            unsigned char hash[SHA_DIGEST_LENGTH];
            uint64_t counter = tid;
            const uint64_t stride = num_threads;
            constexpr int CHECK_INTERVAL = 4096;

            while (true) {
                for (int batch = 0; batch < CHECK_INTERVAL; ++batch) {
                    // Combine authdata with counter
                    std::string data_to_hash = authdata + std::to_string(counter);
                    sha1_raw(reinterpret_cast<const unsigned char*>(data_to_hash.c_str()), 
                             data_to_hash.length(), hash);

                    if (check_difficulty(hash, difficulty)) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found) {
                            found = true;
                            solution = counter;
                        }
                        return;
                    }
                    counter += stride;
                }
                if (found.load(std::memory_order_relaxed)) return;
            }
        };

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; ++i)
            threads.emplace_back(pow_worker, i);
        for (auto &t : threads)
            t.join();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "Solution: " << solution << ", Time: " << elapsed << "ms\n\n";
    }

    void ExasolClient::benchmark_pow2_solving(const std::string &authdata, int difficulty, bool use_multithreading, PowStrategy)
    {
        unsigned int num_threads = use_multithreading ? std::max(1u, std::thread::hardware_concurrency()) : 1;
        std::cout << "POW2 Benchmark (SHA1Precomputed): difficulty=" << difficulty << ", threads=" << num_threads << "\n";

        auto start = std::chrono::high_resolution_clock::now();
        std::atomic<bool> found(false);
        std::atomic<uint64_t> solution(0);
        std::mutex result_mutex;

        // Precompute SHA1 state for authdata (shared across threads)
        SHA1Precomputed precomputed;
        precomputed.init(authdata);

        auto pow_worker = [&](unsigned int tid) {
            unsigned char hash[SHA_DIGEST_LENGTH];
            uint64_t counter = tid;
            const uint64_t stride = num_threads;
            constexpr int CHECK_INTERVAL = 4096;

            while (true) {
                for (int batch = 0; batch < CHECK_INTERVAL; ++batch) {
                    precomputed.hash_counter(counter, hash);

                    if (check_difficulty(hash, difficulty)) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        if (!found) {
                            found = true;
                            solution = counter;
                        }
                        return;
                    }
                    counter += stride;
                }
                if (found.load(std::memory_order_relaxed)) return;
            }
        };

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; ++i)
            threads.emplace_back(pow_worker, i);
        for (auto &t : threads)
            t.join();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "Solution: " << solution << ", Time: " << elapsed << "ms\n\n";
    }

    void ExasolClient::run_all_benchmarks()
    {
        std::cout << "\n=== POW BENCHMARK COMPARISON ===\n";
        std::string authdata = "jkjGGJLLMsyCwEvGXxFXaOnorfQiEaSpjkFprqBAXNuiRdUpKJSsSEQMbiWGXtAk";
        int difficulty = 7;
        
        std::cout << "\n--- Testing POW (sha1_raw) ---\n";
        benchmark_pow_solving(authdata, difficulty, true, PowStrategy::Counter);
        
        std::cout << "\n--- Testing POW2 (SHA1Precomputed) ---\n";
        benchmark_pow2_solving(authdata, difficulty, true, PowStrategy::Counter);
        
        std::cout << "\n=== BENCHMARK COMPLETE ===\n";
    }

    bool ExasolClient::test_sha1_implementations()
    {
        std::cout << "Testing SHA1 implementation correctness...\n";
        
        // Test data
        const std::string test_input = "The quick brown fox jumps over the lazy dog";
        unsigned char hash[SHA_DIGEST_LENGTH];
        
        // Compute hash using sha1_raw
        sha1_raw(reinterpret_cast<const unsigned char*>(test_input.c_str()),
                 test_input.length(), hash);
        
        // Expected SHA1 hash of the test input
        const unsigned char expected[SHA_DIGEST_LENGTH] = {
            0xb0, 0x3b, 0x50, 0x9b, 0x1a, 0x2e, 0x9f, 0x01,
            0x34, 0x21, 0x49, 0xef, 0xad, 0x0c, 0x3f, 0x44,
            0xac, 0x85, 0x0c, 0xee
        };
        
        // Compare
        bool passed = (std::memcmp(hash, expected, SHA_DIGEST_LENGTH) == 0);
        
        if (passed) {
            std::cout << "✓ SHA1 implementation test PASSED\n";
        } else {
            std::cout << "✗ SHA1 implementation test FAILED\n";
        }
        
        return passed;
    }

    void ExasolClient::compare_sha1_performance()
    {
        std::cout << "\nComparing SHA1 performance...\n";
        
        const std::string authdata = "jkjGGJLLMsyCwEvGXxFXaOnorfQiEaSpjkFprqBAXNuiRdUpKJSsSEQMbiWGXtAk";
        const int iterations = 100000;
        unsigned char hash[SHA_DIGEST_LENGTH];
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::string data = authdata + std::to_string(i);
            sha1_raw(reinterpret_cast<const unsigned char*>(data.c_str()),
                     data.length(), hash);
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        
        double throughput = (iterations / static_cast<double>(elapsed)) * 1000.0;
        std::cout << "SHA1 performance: " << iterations << " iterations in "
                  << elapsed << "ms (" << throughput << " hash/sec)\n";
    }

} // namespace exasol
