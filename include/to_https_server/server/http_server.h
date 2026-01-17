#ifndef TO_HTTPS_SERVER_HTTP_SERVER_H
#define TO_HTTPS_SERVER_HTTP_SERVER_H

#include <to_https_server/external/toFileMemory/toFileMemory.h>
#include <to_https_server/server/file_manager.h>
#include <to_https_server/server/security_manager.h>
#include <to_https_server/server/gzip_compressor.h>
#include <to_https_server/utils/logger.h>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <to_https_server/external/httplib.h>

namespace to_https_server {

class http_server {
public:
    http_server();
    ~http_server();
    
    void initialize(const std::string& runtime_dir);
    void start();
    void stop();
    
private:
    void setup_routes();
	bool check_admin_password(const httplib::Request& req) const;
    void handle_file_request(const httplib::Request& req, httplib::Response& res);
    void handle_head_request(const httplib::Request& req, httplib::Response& res);
    void handle_upload_request(const httplib::Request& req, httplib::Response& res);
    void handle_delete_request(const httplib::Request& req, httplib::Response& res);
    void handle_mkdir_request(const httplib::Request& req, httplib::Response& res);
    void handle_list_request(const httplib::Request& req, httplib::Response& res);

	void handle_visits_request(const httplib::Request& req, httplib::Response& res);
    
    void handle_chunked_download(const std::string& path, const httplib::Request& req, httplib::Response& res);
    bool handle_chunked_upload(const httplib::Request& req, httplib::Response& res);
    
    std::string get_client_ip(const httplib::Request& req) const;
    bool should_compress(const std::string& path, size_t size, const std::string& accept_encoding) const;

	std::string query_real_ip(const httplib::Request& req) const;
	std::string query_user_agent(const httplib::Request& req) const;
    
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<file_manager> file_manager_;
    std::unique_ptr<gzip_compressor> compressor_;
    std::unique_ptr<security_manager> security_;
    std::unique_ptr<logger> logger_;
    
    bool running_;
	int port_;
	size_t thread_count_;
	size_t task_queue_size_;
    size_t buffer_chunk_size_;
    size_t max_file_size_;
	size_t cache_max_age_;
	std::string cert_path_;
	std::string privkey_path_;
	std::string admin_password_;
	std::string runtime_dir_;

	toFileMemory visitors_db_;
	uint64_t *visitors_cnt_;
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_HTTP_SERVER_H
