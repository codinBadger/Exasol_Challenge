#ifndef EXASOL_CLIENT_H
#define EXASOL_CLIENT_H

#include "IConfigLoader.h"
#include "ISocketManager.h"
#include "ISSLManager.h"
#include <memory>
#include <string>

namespace exasol {

/**
 * Main client orchestrator
 * Single Responsibility: Coordinates connection workflow
 * Dependency Inversion: Depends on abstractions (interfaces), not concrete implementations
 */
class ExasolClient {
public:
    enum class PowStrategy {
        RandomString,
        RandomHex,
        Counter
    };
    ExasolClient(
        std::unique_ptr<IConfigLoader> config_loader,
        std::unique_ptr<ISocketManager> socket_manager,
        std::unique_ptr<ISSLManager> ssl_manager
    );
    
    ~ExasolClient();
    
    // Open/Closed Principle: Can extend functionality by adding new methods
    // without modifying existing ones
    void connect();
    std::string random_string(size_t length);
    void communicate();
    void disconnect();
    
    std::string get_cipher() const;
    bool is_connected() const;
    
    // Utility function: Compute SHA1 hash and return as hex string
    static std::string sha1_hex(const std::string& input);
    
    // Performance Testing Methods
    static void benchmark_suffix_generation();
    static void benchmark_pow_solving(const std::string& authdata, int difficulty, bool use_multithreading, PowStrategy strategy);
    static void benchmark_pow2_solving(const std::string& authdata, int difficulty, bool use_multithreading, PowStrategy strategy);
    static void run_all_benchmarks();
    
    // SHA-NI Implementation Testing
    static bool test_sha1_implementations();
    static void compare_sha1_performance();

private:
    std::unique_ptr<IConfigLoader> config_loader_;
    std::unique_ptr<ISocketManager> socket_manager_;
    std::unique_ptr<ISSLManager> ssl_manager_;
    
    ClientConfig config_;
    socket_t socket_;
    bool connected_;
};

} // namespace exasol

#endif // EXASOL_CLIENT_H
