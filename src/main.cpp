#include "../include/ExasolClient.h"
#include "../include/FileConfigLoader.h"
#include "../include/CLIConfigLoader.h"
#include "../include/SocketManager.h"
#include "../include/SSLManager.h"
#include <iostream>
#include <memory>
#include <string>

using namespace exasol;

void print_usage() {
    std::cout << "Usage (direct):     ExasolClient <server-address> <port> <ca_cert.pem>\n";
    std::cout << "Usage (config):     ExasolClient --config <config-file>\n";
    std::cout << "Usage (benchmark):  ExasolClient --benchmark [difficulty]\n";
    std::cout << "Usage (test SHA1):  ExasolClient --test-sha1\n";
    std::cout << "\nExamples:\n";
    std::cout << "  ExasolClient 127.0.0.1 8443 cert.pem\n";
    std::cout << "  ExasolClient --config config/client.conf\n";
    std::cout << "  ExasolClient --benchmark      # default difficulty 9\n";
    std::cout << "  ExasolClient --benchmark 6    # custom difficulty\n";
    std::cout << "  ExasolClient --test-sha1\n";
}

int main(int argc, char* argv[]) {
    try {
        // Handle benchmark mode (no network required)
        if (argc >= 2 && std::string(argv[1]) == "--benchmark") {
            std::cout << "\n========================================\n";
            std::cout << "  EXASOL CLIENT - PERFORMANCE BENCHMARK\n";
            std::cout << "========================================\n\n";
            
            int difficulty = 9;  // default
            if (argc >= 3) {
                difficulty = std::stoi(argv[2]);
            }
            
            std::string authdata = "jkjGGJLLMsyCwEvGXxFXaOnorfQiEaSpjkFprqBAXNuiRdUpKJSsSEQMbiWGXtAk";
            
            // Run both POW and POW2 benchmarks
            std::cout << "\n--- Testing POW (sha1_raw) ---\n";
            ExasolClient::benchmark_pow_solving(authdata, difficulty, true, ExasolClient::PowStrategy::Counter);
            
            std::cout << "\n--- Testing POW2 (SHA1Precomputed) ---\n";
            ExasolClient::benchmark_pow2_solving(authdata, difficulty, true, ExasolClient::PowStrategy::Counter);
            
            std::cout << "\n========================================\n";
            std::cout << "  BENCHMARK COMPARISON COMPLETE\n";
            std::cout << "========================================\n\n";
            return 0;
        }
        
        // Handle SHA1 implementation test mode
        if (argc == 2 && std::string(argv[1]) == "--test-sha1") {
            std::cout << "\n========================================\n";
            std::cout << "  EXASOL CLIENT - SHA1 IMPLEMENTATION TEST\n";
            std::cout << "========================================\n";
            bool passed = ExasolClient::test_sha1_implementations();
            ExasolClient::compare_sha1_performance();
            return passed ? 0 : 1;
        }
        
        std::unique_ptr<IConfigLoader> config_loader;
        
        // Parse command-line arguments and create appropriate config loader
        if (argc == 3 && std::string(argv[1]) == "--config") {
            // Config file mode
            config_loader = std::make_unique<FileConfigLoader>(argv[2]);
        } 
        else if (argc == 4) {
            // CLI mode
            std::string address = argv[1];
            uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
            std::string ca_cert = argv[3];
            config_loader = std::make_unique<CLIConfigLoader>(address, port, ca_cert);
        } 
        else {
            print_usage();
            return 1;
        }
        
        // Create dependencies (Dependency Injection)
        auto socket_manager = std::make_unique<SocketManager>();
        auto ssl_manager = std::make_unique<SSLManager>();
        
        // Create client with injected dependencies
        ExasolClient client(
            std::move(config_loader),
            std::move(socket_manager),
            std::move(ssl_manager)
        );
        
        // Execute workflow
        client.connect();
        client.communicate();
        client.disconnect();
        
        std::cout << "Client finished successfully.\n";
        return 0;
        
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
