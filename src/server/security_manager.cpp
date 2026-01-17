#include <to_https_server/server/security_manager.h>
#include <to_https_server/server/config.h>
#include <chrono>

namespace to_https_server {

security_manager::security_manager() : attack_mode_(false) {}

bool security_manager::is_under_attack() const {
    return attack_mode_.load();
}

bool security_manager::should_block_request(const std::string& client_ip) {
    if (attack_mode_.load()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& client = clients_[client_ip];
    
    if (std::chrono::duration_cast<std::chrono::seconds>(
            now - client.last_request).count() > 1) {
        client.request_count = 0;
    }
    
    client.last_request = now;
    client.request_count++;
    
    const auto& config = config_manager::instance().get_config();
    
    if (client.request_count > config.max_requests_per_second) {
        size_t total_requests = 0;
        for (const auto& [ip, info] : clients_) {
            total_requests += info.request_count;
        }
        
        if (total_requests > config.attack_threshold) {
            enter_attack_mode();
            return true;
        }
    }
    
    return false;
}

void security_manager::enter_attack_mode() {
    attack_mode_.store(true);
}

void security_manager::exit_attack_mode() {
    attack_mode_.store(false);
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.clear();
}

bool security_manager::is_attack_mode() const {
    return attack_mode_.load();
}

void security_manager::cleanup_old_clients() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    for (auto it = clients_.begin(); it != clients_.end();) {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_request).count() > 60) {
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace to_https_server
