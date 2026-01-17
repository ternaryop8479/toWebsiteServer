#include <to_https_server/server/config.h>
#include <fstream>
#include <iostream>

namespace to_https_server {

config_manager& config_manager::instance() {
    static config_manager instance;
    return instance;
}

void config_manager::load_config(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open config file " << config_path << ", using defaults\n";
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 去除空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
		if(value.back() == '\n') { // 解决getline会额外读取换行的问题
			value = value.substr(0, value.size() - 1);
		}
        
        if (key == "port") config_.port = std::stoi(value);
        else if (key == "ssl_cert_path") config_.ssl_cert_path = value;
        else if (key == "ssl_key_path") config_.ssl_key_path = value;
        else if (key == "thread_pool_size") config_.thread_pool_size = std::stoi(value);
        else if (key == "task_queue_size") config_.task_queue_size = std::stoi(value);
        // else if (key == "max_requests_per_second") config_.max_requests_per_second = std::stoi(value);
        // else if (key == "attack_threshold") config_.attack_threshold = std::stoi(value);
        else if (key == "www_root") config_.www_root = value;
        else if (key == "log_dir") config_.log_dir = value;
        else if (key == "trash_dir") config_.trash_dir = value;
        else if (key == "buffer_chunk_size") config_.buffer_chunk_size = std::stoull(value);
        else if (key == "max_file_size") config_.max_file_size = std::stoull(value);
		else if (key == "cache_max_age") config_.cache_max_age = std::stoull(value);
		else if (key == "admin_password") config_.admin_password = value;
    }
}

const server_config& config_manager::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

} // namespace to_https_server
