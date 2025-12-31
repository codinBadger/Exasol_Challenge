#include "../include/ExasolClient.h"
#include <iostream>
#include <stdexcept>
#include <cstring>  // for memset
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
#include "ExasolClient.h"

namespace exasol {

ExasolClient::ExasolClient(
    std::unique_ptr<IConfigLoader> config_loader,
    std::unique_ptr<ISocketManager> socket_manager,
    std::unique_ptr<ISSLManager> ssl_manager
) : config_loader_(std::move(config_loader)),
    socket_manager_(std::move(socket_manager)),
    ssl_manager_(std::move(ssl_manager)),
    socket_(0),
    connected_(false) {}

ExasolClient::~ExasolClient() {
    if (connected_) {
        disconnect();
    }
}

std::string ExasolClient::sha1_hex(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

void ExasolClient::connect() {
    try {
        // Load configuration
        config_ = config_loader_->load();

        // Normalize ports list (support legacy single-port configs)
        if (config_.ports.empty() && config_.port != 0) {
            config_.ports.push_back(config_.port);
        }
        if (config_.ports.empty()) {
            throw std::runtime_error("No ports provided in configuration");
        }
        
        const int max_attempts = 10;
        const auto retry_delay = std::chrono::seconds(3);

        // Initialize SSL context once; SSL objects reset per attempt in attach_socket
        ssl_manager_->initialize(config_.ca_cert, config_.server_name);
        if (!config_.client_cert.empty() && !config_.client_key.empty()) {
            ssl_manager_->load_client_certificate(config_.client_cert, config_.client_key);
        }

        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            uint16_t port_to_try = config_.ports[(attempt - 1) % config_.ports.size()];
            try {
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
            } catch (const std::exception& ex) {
                if (socket_manager_->is_valid(socket_)) {
                    socket_manager_->close(socket_);
                }
                socket_ = 0;

                if (attempt == max_attempts) {
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
    } catch (const std::exception& ex) {
        if (socket_manager_->is_valid(socket_)) {
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
    if (result.length() > length) {
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
    for (int c = 33; c <= 126; ++c) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            allowed += static_cast<char>(c);
        }
    }
    
    std::uniform_int_distribution<> dis(0, allowed.length() - 1);
    std::string result;
    for (size_t i = 0; i < length; ++i) {
        result += allowed[dis(gen)];
    }
    return result;
}
void ExasolClient::communicate() {
    if (!connected_) {
        throw std::runtime_error("Not connected to server");
    }
    
    try {
        char buffer[4096]{};
        std::string authdata;
        bool authenticated = false; 
        while (true) {
            int bytes_received = ssl_manager_->read(buffer, sizeof(buffer) - 1);
            
            if (bytes_received <= 0) {
                std::cout << "Connection closed or error occurred." << std::endl;
                break;
            }
            
            buffer[bytes_received] = '\0';
            std::string data(buffer, bytes_received);
            
            // Trim trailing whitespace
            size_t end = data.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                data.resize(end + 1);
            }
            
            std::cout << "Server command: " << data << "\n";
            
            // Split by space to get args
            std::vector<std::string> args;
            std::istringstream iss(data);
            std::string token;
            while (iss >> token) {
                args.push_back(token);
            }
            
            if (args.empty()) {
                std::cout << "Empty command received\n";
                continue;
            }
            
            // Prepare response based on command
            std::string response;
            if (args[0] == "HELO") {
                response = "EHLO\n";
                std::cout << "Responding with: EHLO\n";
            } 
            else if (args[0] == "ERROR") {
                std::cout << "ERROR: ";
                for (size_t i = 1; i < args.size(); ++i) {
                    std::cout << args[i];
                    if (i < args.size() - 1) std::cout << " ";
                }
                std::cout << "\n";
                break;
            } 
            else if (args[0] == "POW") {
                if (args.size() >= 3) {
                    authdata = args[1];
                    int difficulty = std::stoi(args[2]);
                    std::cout << "POW command received with authdata=" << authdata 
                              << ", difficulty=" << difficulty << "\n";
                    
                    // Multithreaded POW solver (counter-partitioned for no duplicates)
                    std::atomic<bool> found(false);
                    std::mutex result_mutex;
                    std::string found_suffix;

                    const unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
                    const uint64_t stride = static_cast<uint64_t>(num_threads);

                    auto pow_worker = [&](unsigned int thread_id) {
                        uint64_t local_counter = thread_id;
                        while (!found.load(std::memory_order_relaxed)) {
                            std::string suffix = generate_suffix(local_counter);
                            std::string target = sha1_hex(authdata + suffix);

                            bool has_leading_zeros = true;
                            for (int i = 0; i < difficulty; ++i) {
                                if (target[i] != '0') {
                                    has_leading_zeros = false;
                                    break;
                                }
                            }

                            if (has_leading_zeros) {
                                std::lock_guard<std::mutex> lock(result_mutex);
                                if (!found) {
                                    found = true;
                                    found_suffix = suffix;
                                    std::cout << "[Thread " << thread_id << "] Found valid suffix: "
                                              << suffix << " with hash: " << target << "\n";
                                }
                                break;
                            }

                            local_counter += stride;  // partitioned search space
                        }
                    };

                    std::vector<std::thread> threads;
                    threads.reserve(num_threads);
                    for (unsigned int i = 0; i < num_threads; ++i) {
                        threads.emplace_back(pow_worker, i);
                    }
                    for (auto& thread : threads) {
                        thread.join();
                    }

                    response = found_suffix + "\n";
                    authenticated = true;
                } else {
                    response = "POW_ERROR: Insufficient arguments\n";
                }
            } 
            else if (args[0] == "END") {
                // Data submission complete
                response = "OK\n";
                std::cout << "Received END command, finishing communication.\n";
           }
            else if (args[0] == "NAME") {
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string name_hash = sha1_hex(authdata + challenge);
                    response = name_hash + " " + "Deepak Shivanandham\n";
                    std::cout << "Responding to NAME\n";
                } else {
                    response = "ERROR: NAME requires authentication\n";
                }
            } 
            else if (args[0] == "MAILNUM") {
               if(authenticated && args.size() >=2){
                   std::string challenge = args[1];
                   std::string mailnum_hash = sha1_hex(authdata + challenge);
                   response = mailnum_hash + " " + "1\n";
                   std::cout << "Responding to MAILNUM\n";
               } else {
                   response = "ERROR: MAILNUM requires authentication\n";
               }
            } 
             else if (args[0] == "MAIL1") {
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "deepakshivanandham@hotmail.com\n";
                    std::cout << "Responding to MAIL1\n";
                } else {
                    response = "ERROR: MAIL1 requires authentication\n";
                }
            } 
            else if (args[0] == "SKYPE") {
                // Specify Skype account for the interview, or N/A if no account
                if ( authenticated && args.size() >= 2 ) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "NA\n";
                } else {
                    response = "ERROR: SKYPE requires authdata\n";
                }
            } 
            else if (args[0] == "BIRTHDATE") {
                // Specify birthdate in format %d.%m.%Y
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "06.02.1991\n";
                } else {
                    response = "ERROR: BIRTHDATE requires authdata\n";
                }
            } 
            else if (args[0] == "COUNTRY") {
                // Country where you currently live
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "india\n";
                } else {
                    response = "ERROR: COUNTRY requires authdata\n";
                }
            } 
            else if (args[0] == "ADDRNUM") {
                // Specifies how many lines your address has
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "2\n";
                } else {
                    response = "ERROR: ADDRNUM requires authdata\n";
                }
            } 
            else if (args[0] == "ADDRLINE1") {
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "25, GAJALAKSHMI NAGAR 1st CROSS STREET\n";
                } else {
                    response = "ERROR: ADDRLINE1 requires authdata\n";
                }
            } 
            else if (args[0] == "ADDRLINE2") {
                if (authenticated && args.size() >= 2) {
                    std::string challenge = args[1];
                    std::string hash = sha1_hex(authdata + challenge);
                    response = hash + " " + "CHROMPET,CHENNAI, TAMILNADU\n";
                } else {
                    response = "ERROR: ADDRLINE2 requires authdata\n";
                }
            } 
            else {
                response = "ERROR Unknown command\n";
            }
            
            // Send response
            if (!response.empty()) {
                ssl_manager_->write(response);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Communication error: " << e.what() << std::endl;
    }
}

void ExasolClient::disconnect() {
    if (connected_) {
        ssl_manager_->shutdown();
        socket_manager_->close(socket_);
        connected_ = false;
        std::cout << "Client disconnected.\n";
    }
}

std::string ExasolClient::get_cipher() const {
    return ssl_manager_->get_cipher();
}

bool ExasolClient::is_connected() const {
    return connected_;
}

// ============================================================================
// PERFORMANCE BENCHMARKING METHODS
// ============================================================================

void ExasolClient::benchmark_suffix_generation() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "SUFFIX GENERATION BENCHMARK\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    const int iterations = 100000;

    ExasolClient dummy(nullptr, nullptr, nullptr);

    auto run_speed_bench = [&](auto generator) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile auto result = generator(i);
            (void)result;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double per_us = duration_ms * 1000.0 / iterations;
        double throughput = iterations * 1000.0 / std::max<int64_t>(1, duration_ms);
        std::cout << "  Time: " << duration_ms << "ms for " << iterations << " iterations\n";
        std::cout << "  Per suffix: " << per_us << " µs\n";
        std::cout << "  Throughput: " << throughput << " suffixes/sec\n";
        return duration_ms;
    };

    std::cout << "Test 1: random_string() (ASCII random, 16 chars)\n";
    auto duration1 = run_speed_bench([&](int) { return dummy.random_string(16); });
    std::cout << "\n";

    std::cout << "Test 2: random_hex_string() (Hex random, 8 chars)\n";
    auto duration2 = run_speed_bench([&](int) { return random_hex_string(); });
    std::cout << "  Speedup vs random_string(): " << std::fixed << std::setprecision(2)
              << (duration1 * 1.0 / std::max<int64_t>(1, duration2)) << "x\n\n";

    std::cout << "Test 3: generate_suffix() (Counter-based, deterministic)\n";
    auto duration3 = run_speed_bench([&](int i) { return generate_suffix(static_cast<uint64_t>(i)); });
    std::cout << "  Speedup vs random_string(): " << std::fixed << std::setprecision(2)
              << (duration1 * 1.0 / std::max<int64_t>(1, duration3)) << "x\n";
    std::cout << "  Speedup vs random_hex_string(): " << std::fixed << std::setprecision(2)
              << (duration2 * 1.0 / std::max<int64_t>(1, duration3)) << "x\n\n";

    std::cout << "SUMMARY:\n";
    std::cout << "  1. random_string():        " << std::setw(8) << duration1 << "ms (1.00x baseline)\n";
    std::cout << "  2. random_hex_string():    " << std::setw(8) << duration2 << "ms ("
              << std::fixed << std::setprecision(2) << (duration1 * 1.0 / std::max<int64_t>(1, duration2)) << "x faster)\n";
    std::cout << "  3. generate_suffix():      " << std::setw(8) << duration3 << "ms ("
              << std::fixed << std::setprecision(2) << (duration1 * 1.0 / std::max<int64_t>(1, duration3)) << "x faster)\n\n";

    // SHA1 leading-zero search performance across difficulties 1-6 (all strategies)
    const std::string base_data = "benchmark-data";

    auto leading_zero_search = [&](const std::string& label, auto generator) {
        std::cout << label << " leading-zero search (difficulties 1-6)\n";
        for (int difficulty = 1; difficulty <= 6; ++difficulty) {
            auto pow_start = std::chrono::high_resolution_clock::now();
            uint64_t attempts = 0;
            std::string found_suffix;

            while (true) {
                std::string suffix = generator();
                std::string hash = sha1_hex(base_data + suffix);

                bool has_leading_zeros = true;
                for (int i = 0; i < difficulty; ++i) {
                    if (hash[i] != '0') {
                        has_leading_zeros = false;
                        break;
                    }
                }

                ++attempts;
                if (has_leading_zeros) {
                    found_suffix = suffix;
                    break;
                }
            }

            auto pow_end = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(pow_end - pow_start).count();
            double rate = elapsed_us > 0 ? attempts * 1'000'000.0 / elapsed_us : attempts * 1'000'000.0;

            std::cout << "  Difficulty " << difficulty << ": "
                      << std::fixed << std::setprecision(2) << (elapsed_us / 1000.0) << " ms"
                      << ", iterations=" << attempts
                      << ", rate=" << rate << " attempts/sec"
                      << ", suffix=" << found_suffix << "\n";
        }
        std::cout << "\n";
    };

    leading_zero_search("Counter", [&]() {
        static uint64_t counter = 0;
        return generate_suffix(counter++);
    });

    leading_zero_search("Random string", [&]() {
        return dummy.random_string(16);
    });

    leading_zero_search("Random hex", [&]() {
        return random_hex_string();
    });
}

static std::string make_suffix_from_strategy(ExasolClient::PowStrategy strategy, uint64_t counter, ExasolClient& dummy) {
    switch (strategy) {
        case ExasolClient::PowStrategy::RandomString:
            return dummy.random_string(16);
        case ExasolClient::PowStrategy::RandomHex:
            return random_hex_string();
        case ExasolClient::PowStrategy::Counter:
        default:
            return generate_suffix(counter);
    }
}

void ExasolClient::benchmark_pow_solving(const std::string& authdata, int difficulty, bool use_multithreading, PowStrategy strategy) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "POW SOLVING BENCHMARK\n";
    std::cout << "Authdata: " << authdata << " | Difficulty: " << difficulty 
              << " | Multithreading: " << (use_multithreading ? "YES" : "NO")
              << " | Strategy: " << (strategy == PowStrategy::RandomString ? "random_string" :
                                     strategy == PowStrategy::RandomHex ? "random_hex_string" : "counter")
              << "\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    std::atomic<bool> found(false);
    std::string found_suffix;
    uint64_t iterations = 0;
    ExasolClient dummy(nullptr, nullptr, nullptr);
    
    if (use_multithreading) {
        // Multithreaded version
        std::mutex result_mutex;
        unsigned int num_threads = std::max(4u, std::thread::hardware_concurrency());
        std::cout << "Using " << num_threads << " threads\n\n";
        
        auto pow_worker = [&](int thread_id) {
            uint64_t local_counter = thread_id;
            uint64_t local_iterations = 0;
            while (!found) {
                std::string suffix = (strategy == PowStrategy::Counter)
                    ? generate_suffix(local_counter)
                    : make_suffix_from_strategy(strategy, local_counter, dummy);
                std::string target = sha1_hex(authdata + suffix);

                bool has_leading_zeros = true;
                for (int i = 0; i < difficulty; ++i) {
                    if (target[i] != '0') {
                        has_leading_zeros = false;
                        break;
                    }
                }

                if (has_leading_zeros && !found) {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    if (!found) {
                        found = true;
                        found_suffix = suffix;
                        iterations = local_counter;
                        std::cout << "[Thread " << thread_id << "] Found: " << suffix 
                                  << " with hash: " << target << "\n";
                    }
                    break;
                }

                local_counter += (strategy == PowStrategy::Counter ? num_threads : 1);
                local_iterations++;
            }
        };
        
        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(pow_worker, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    } else {
        // Single-threaded version
        std::cout << "Using 1 thread (single-threaded)\n\n";
        
        uint64_t counter = 0;
        while (!found) {
            std::string suffix = make_suffix_from_strategy(strategy, counter, dummy);
            std::string target = sha1_hex(authdata + suffix);
            bool has_leading_zeros = true;
            for (int i = 0; i < difficulty; ++i) {
                if (target[i] != '0') {
                    has_leading_zeros = false;
                    break;
                }
            }
            if (has_leading_zeros) {
                found = true;
                found_suffix = suffix;
                iterations = counter;
                std::cout << "Found: " << suffix << " with hash: " << target << "\n";
                break;
            }
            counter += (strategy == PowStrategy::Counter ? 1 : 1); // same step for clarity
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "\nResults:\n";
    std::cout << "  Solution:         " << found_suffix << "\n";
    std::cout << "  Time elapsed:     " << duration << " ms\n";
    std::cout << "  Iterations:       " << iterations << "\n";
    std::cout << "  Rate:             " << (iterations * 1000.0 / duration) << " attempts/sec\n\n";
}

void ExasolClient::run_all_benchmarks() {
    std::cout << "\n\n";
    std::cout << "+" << std::string(68, '-') << "+\n";
    std::cout << "|" << std::string(15, ' ') << "EXASOL CLIENT PERFORMANCE BENCHMARKS" << std::string(17, ' ') << "|\n";
    std::cout << "|" << std::string(15, ' ') << "Phase 3: Optimization & Multithreading" << std::string(14, ' ') << "|\n";
    std::cout << "+" << std::string(68, '-') << "+\n";
    
    // Test suffix generation performance
    benchmark_suffix_generation();
    
    // Test POW solving with different configurations
    std::cout << "\n\nTesting POW Solving Performance:\n";
    
    // Single-threaded baselines for each strategy
    benchmark_pow_solving("testdata123", 5, false, PowStrategy::RandomString);
    benchmark_pow_solving("testdata123", 5, false, PowStrategy::RandomHex);
    benchmark_pow_solving("testdata123", 5, false, PowStrategy::Counter);

    // Multithreaded versions for each strategy
    benchmark_pow_solving("testdata123", 5, true, PowStrategy::RandomString);
    benchmark_pow_solving("testdata123", 5, true, PowStrategy::RandomHex);
    benchmark_pow_solving("testdata123", 5, true, PowStrategy::Counter);
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "BENCHMARKS COMPLETE\n";
    std::cout << std::string(70, '=') << "\n\n";
}

} // namespace exasol
