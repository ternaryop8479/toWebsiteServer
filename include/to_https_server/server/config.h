#ifndef TO_HTTPS_SERVER_CONFIG_H
#define TO_HTTPS_SERVER_CONFIG_H

#include <string>
#include <atomic>
#include <mutex>

namespace to_https_server {

struct server_config {
    // 网络配置
    int port = 443;
	// 默认全部留空，为http
    std::string ssl_cert_path = "";
    std::string ssl_key_path = "";
    
    // 线程池配置
    size_t thread_pool_size = 8;
    size_t task_queue_size = 1000;
    
    // 攻击检测(Useless now)
    size_t max_requests_per_second = 1000;
    size_t attack_threshold = 10000;
    
    // 文件配置
    std::string www_root = "www";
    std::string log_dir = "logs";
    std::string trash_dir = "trashfiles";
    
    // 传输配置
    size_t buffer_chunk_size = 5 * 1024 * 1024; // 5MB
    size_t max_file_size = 2ULL * 1024 * 1024 * 1024; // 2GB
	size_t cache_max_age = 14400; // 4 hours
    
    // 运行时目录
    std::string runtime_dir = ".";

	// 管理员密码
	std::string admin_password = "123456";
};

class config_manager {
public:
    static config_manager& instance();
    
    void load_config(const std::string& config_path);
    const server_config& get_config() const;
    
private:
    config_manager() = default;
    ~config_manager() = default;
    
    server_config config_;
    mutable std::mutex config_mutex_;
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_CONFIG_H
