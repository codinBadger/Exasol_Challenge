#include "../include/SSLManager.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace exasol {

class SSLManager::Impl {
public:
    Impl() : ctx_(nullptr), ssl_(nullptr) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
    }
    
    ~Impl() {
        if (ssl_) {
            SSL_free(ssl_);
        }
        if (ctx_) {
            SSL_CTX_free(ctx_);
        }
        EVP_cleanup();
    }
    
    void initialize(const std::string& ca_cert_path, const std::string& server_name) {
        const SSL_METHOD* method = TLS_client_method();
        ctx_ = SSL_CTX_new(method);
        if (!ctx_) {
            throw std::runtime_error("Unable to create SSL context");
        }
        
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_verify_depth(ctx_, 4);
        
        if (SSL_CTX_load_verify_locations(ctx_, ca_cert_path.c_str(), nullptr) != 1) {
            throw std::runtime_error("Failed to load CA certificate for verification");
        }

        if (!server_name.empty()) {
            // Enable hostname verification
            X509_VERIFY_PARAM* param = SSL_CTX_get0_param(ctx_);
            X509_VERIFY_PARAM_set_hostflags(param, 0);
            if (X509_VERIFY_PARAM_set1_host(param, server_name.c_str(), 0) != 1) {
                throw std::runtime_error("Failed to set server hostname for verification");
            }
            server_name_ = server_name;
        } else {
            server_name_.clear();
        }
    }

    void load_client_certificate(const std::string& cert_path, const std::string& key_path) {
        if (cert_path.empty() || key_path.empty()) {
            return; // optional
        }
        if (!ctx_) {
            throw std::runtime_error("SSL context not initialized");
        }

        if (SSL_CTX_use_certificate_chain_file(ctx_, cert_path.c_str()) != 1) {
            throw std::runtime_error("Failed to load client certificate chain");
        }
        if (SSL_CTX_use_PrivateKey_file(ctx_, key_path.c_str(), SSL_FILETYPE_PEM) != 1) {
            throw std::runtime_error("Failed to load client private key");
        }
        if (SSL_CTX_check_private_key(ctx_) != 1) {
            throw std::runtime_error("Client private key does not match certificate");
        }
    }
    
    void attach_socket(socket_t socket) {
        if (!ctx_) {
            throw std::runtime_error("SSL context not initialized");
        }
        
        if (ssl_) {
            SSL_free(ssl_);
            ssl_ = nullptr;
        }

        ssl_ = SSL_new(ctx_);
        if (!ssl_) {
            throw std::runtime_error("Failed to create SSL structure");
        }
        
        SSL_set_fd(ssl_, static_cast<int>(socket));

        if (!server_name_.empty()) {
            SSL_set_tlsext_host_name(ssl_, server_name_.c_str());
        }
    }
    
    void handshake() {
        if (!ssl_) {
            throw std::runtime_error("SSL not attached to socket");
        }
        
        if (SSL_connect(ssl_) <= 0) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("TLS handshake failed (certificate mismatch?)");
        }
    }
    
    std::string read() {
        if (!ssl_) {
            throw std::runtime_error("SSL not initialized");
        }
        
        char buffer[256]{};
        int bytes = SSL_read(ssl_, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            return std::string(buffer);
        }
        return "";
    }
    
    int read(char* buffer, int size) {
        if (!ssl_) {
            throw std::runtime_error("SSL not initialized");
        }
        
        return SSL_read(ssl_, buffer, size);
    }
    
    void write(const std::string& data) {
        if (!ssl_) {
            throw std::runtime_error("SSL not initialized");
        }
        
        SSL_write(ssl_, data.c_str(), static_cast<int>(data.length()));
    }
    
    std::string get_cipher() const {
        if (!ssl_) {
            return "Not connected";
        }
        const char* cipher = SSL_get_cipher(ssl_);
        return cipher ? cipher : "Unknown";
    }
    
    void shutdown() {
        if (ssl_) {
            SSL_shutdown(ssl_);
        }
    }

private:
    SSL_CTX* ctx_;
    SSL* ssl_;
    std::string server_name_;
};

SSLManager::SSLManager() : pImpl_(std::make_unique<Impl>()) {}

SSLManager::~SSLManager() = default;

void SSLManager::initialize(const std::string& ca_cert_path, const std::string& server_name) {
    pImpl_->initialize(ca_cert_path, server_name);
}

void SSLManager::load_client_certificate(const std::string& cert_path, const std::string& key_path) {
    pImpl_->load_client_certificate(cert_path, key_path);
}

void SSLManager::attach_socket(socket_t socket) {
    pImpl_->attach_socket(socket);
}

void SSLManager::handshake() {
    pImpl_->handshake();
}

std::string SSLManager::read() {
    return pImpl_->read();
}

int SSLManager::read(char* buffer, int size) {
    return pImpl_->read(buffer, size);
}

void SSLManager::write(const std::string& data) {
    pImpl_->write(data);
}

std::string SSLManager::get_cipher() const {
    return pImpl_->get_cipher();
}

void SSLManager::shutdown() {
    pImpl_->shutdown();
}

} // namespace exasol
