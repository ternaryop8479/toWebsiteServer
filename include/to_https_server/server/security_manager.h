#ifndef TO_HTTPS_SERVER_SECURITY_MANAGER_H
#define TO_HTTPS_SERVER_SECURITY_MANAGER_H

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <string>

namespace to_https_server {

class security_manager {
public:
    security_manager();
    
    bool is_under_attack() const;
    bool should_block_request(const std::string& client_ip);
    
    void enter_attack_mode();
    void exit_attack_mode();
    
    bool is_attack_mode() const;
    
private:
    struct client_info {
        std::chrono::steady_clock::time_point last_request;
        size_t request_count;
    };
    
    std::atomic<bool> attack_mode_;
    std::unordered_map<std::string, client_info> clients_;
    mutable std::mutex clients_mutex_;
    
    void cleanup_old_clients();
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_SECURITY_MANAGER_H
