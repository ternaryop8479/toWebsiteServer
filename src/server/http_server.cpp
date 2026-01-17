#include <to_https_server/server/config.h>
#include <to_https_server/server/http_server.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <cstring>

namespace fs = std::filesystem;

namespace to_https_server {

http_server::http_server() : running_(false) {}

http_server::~http_server() {
    stop();
}

void http_server::initialize(const std::string& runtime_dir) {
    auto& config = config_manager::instance();

    // 设置运行时目录
	runtime_dir_ = runtime_dir;
	if(runtime_dir_.back() == '/') {
		runtime_dir_.erase(runtime_dir_.end() - 1);
	}
    std::string config_path = runtime_dir + "/config/server.conf";
    config.load_config(config_path);

    const auto& server_config = config.get_config();

    // 创建必要的目录
    std::string www_path = server_config.www_root;
    std::string log_path = server_config.log_dir;
    std::string trash_path = server_config.trash_dir;

    file_manager_ = std::make_unique<file_manager>(www_path, trash_path);
    compressor_ = std::make_unique<gzip_compressor>();
    security_ = std::make_unique<security_manager>();
    logger_ = std::make_unique<logger>(log_path);

	thread_count_ = server_config.thread_pool_size;
	task_queue_size_ = server_config.task_queue_size;
    buffer_chunk_size_ = server_config.buffer_chunk_size;
    max_file_size_ = server_config.max_file_size;
	cache_max_age_ = server_config.cache_max_age;

	cert_path_ = server_config.ssl_cert_path;
	privkey_path_ = server_config.ssl_key_path;

	port_ = server_config.port;

	admin_password_ = server_config.admin_password;

	// 创建需要用的数据库
	visitors_cnt_ = reinterpret_cast<uint64_t*>(visitors_db_.open((runtime_dir_ + "/.visitors.db").c_str(), 8));
}

void http_server::start() {
    if (running_) {
        return;
    }
    
	if (!cert_path_.empty() && !privkey_path_.empty())
		server_ = std::make_unique<httplib::SSLServer>(cert_path_.c_str(), privkey_path_.c_str());
	else
		server_ = std::make_unique<httplib::Server>();

    if (!server_->is_valid()) {
        logger_->log(logger::level::error, "Failed to create server");
        return;
    }

	// 服务器线程池设置
	server_->new_task_queue = [this] {
		return new httplib::ThreadPool(/*线程数*/thread_count_, /*任务队列大小*/task_queue_size_);
	};
    
    setup_routes();
    
    running_ = true;
    logger_->log(logger::level::info, "Server starting on port " + std::to_string(port_));
    
    // 阻塞监听
    if (!server_->listen("0.0.0.0", port_)) {
        logger_->log(logger::level::error, "Failed to start server on port " + std::to_string(port_));
    }
    
    running_ = false;
    logger_->log(logger::level::info, "Server stopped");
}

void http_server::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    if (server_) {
        server_->stop();
    }
    
    logger_->log(logger::level::info, "Server stopped");
}

std::string http_server::query_real_ip(const httplib::Request& req) const {
	std::string result = "Unknown_IP";
	if(req.has_header("CF-Connecting-IP")) {
		result = req.get_header_value("CF-Connecting-IP");
	}
	return result;
}

std::string http_server::query_user_agent(const httplib::Request& req) const {
	std::string result = "Unknown_UserAgent";
	if(req.has_header("User-Agent")) {
		result = req.get_header_value("User-Agent");
	}
	return result;
}

void http_server::setup_routes() {
    server_->set_pre_routing_handler([this](const auto& req, auto& res) {
        std::string client_ip = get_client_ip(req);
        
        if (security_->is_under_attack()) {
            res.status = 503;
            res.set_content("服务器正在被攻击，将暂时停止服务/Server is under attack and will temporarily suspend service.", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        
        if (security_->should_block_request(client_ip)) {
            res.status = 429;
            res.set_content("Too many requests", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        
        // 检查上传文件大小限制
        if (req.method == "POST" || req.method == "PUT") {
            auto content_length = req.get_header_value("Content-Length");
            if (!content_length.empty()) {
                size_t size = std::stoull(content_length);
                if (size > max_file_size_) {
                    res.status = 413;
                    res.set_content("File too large", "text/plain");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
        }
        
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    server_->Get(".*", [this](const auto& req, auto& res) {
		std::string real_ip = query_real_ip(req);
		std::string user_agent = query_user_agent(req);
		logger_->log(logger::level::info, "Accepted a GET request from ip " + real_ip + ", path: " + req.path + ", user-agent: " + user_agent);
		if(req.path == "/api/visits") {
			handle_visits_request(req, res);
			return;
		}
		handle_file_request(req, res);
		logger_->log(logger::level::info, "The response for a GET request sent. Code: " + std::to_string(res.status == -1 ? 200 : res.status) + ", Request path: " + req.path);
    });
    
    // server_->Head(".*", [this](const auto& req, auto& res) {
    //     handle_head_request(req, res);
    // });
    
    server_->Post(".*", [this](const auto& req, auto& res) {
		std::string real_ip = query_real_ip(req);
		std::string user_agent = query_user_agent(req);
		if(!req.has_param("param")) {
			res.status = 400;
			res.set_content("No param provided.", "text/plain");
			return;
		}
        std::string param = req.get_param_value("param");
		logger_->log(logger::level::info, "Accepted a POST request from ip " + real_ip + ", parameter: " + param + ", path: " + req.path + ", user-agent: " + user_agent);
        if (param == "upload") {
            handle_upload_request(req, res);
        } else if (param == "delete") {
            handle_delete_request(req, res);
        } else if (param == "mkdir") {
            handle_mkdir_request(req, res);
        } else if (param == "ergodic") {
            handle_list_request(req, res);
        } else {
            handle_file_request(req, res);
        }
		logger_->log(logger::level::info, "The response for a POST request sent. Code: " + std::to_string(res.status == -1 ? 200 : res.status) + ", Request path: " + req.path);
    });
    
    server_->Put(".*", [this](const auto& req, auto& res) {
        // 处理大文件上传
        if (handle_chunked_upload(req, res)) {
            return;
        }
        
        // 普通上传
        handle_upload_request(req, res);
    });
}

void http_server::handle_visits_request(const httplib::Request& req, httplib::Response& res) {
	(void)req;
	res.status = 200;
	res.set_content(std::to_string(*visitors_cnt_), "text/plain");
}

void http_server::handle_file_request(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string path = req.path;

        // 处理查询参数
        size_t param_pos = path.find('?');
        if (param_pos != std::string::npos) {
            path = path.substr(0, param_pos);
        }
        
        std::string safe_path = file_manager_->sanitize_path(path);

		// 云盘特殊处理，对于cloud-drive目录和其所有子目录都返回cloud-drive.html
		if(path.find("/cloud-drive") != std::string::npos && fs::is_directory(safe_path)) {
			logger_->log(logger::level::info, "Client is visiting a directory but in cloud drive.");
			++(*visitors_cnt_);
			path = "/cloud-drive.html";
			safe_path = file_manager_->sanitize_path(path);
		}

        if (file_manager_->is_directory(safe_path)) {
			std::string index_path = safe_path + "/index.html";
			if (file_manager_->file_exists(index_path)) {
				logger_->log(logger::level::info, "index.html find.");
				++(*visitors_cnt_);
				safe_path = index_path;
			} else {
				res.status = 404;
				res.set_content("Directory listing disabled here", "text/plain");
				return;
			}
		}
		logger_->log(logger::level::info, "Final path: " + safe_path);
        
        if (!file_manager_->file_exists(safe_path)) {
            res.status = 404;
            std::string default_404 = file_manager_->sanitize_path("/404.html");
            if (file_manager_->file_exists(default_404)) {
                std::string content;
                file_manager_->read_file(default_404, content);
                res.set_content(content, "text/html");
            } else {
				logger_->log(logger::level::info, "Ret code");
                res.set_content("404 Not Found", "text/plain");
            }
            return;
        }
        
        size_t file_size = file_manager_->get_file_size(safe_path);
        std::string content_type = file_manager_->get_content_type(safe_path);
        auto accept_encoding = req.get_header_value("Accept-Encoding");

        // 处理Range请求
        auto range_header = req.get_header_value("Range");
        if (!range_header.empty()) {
            // 解析Range头
            std::string range_str = range_header.substr(6); // "bytes="
            size_t dash_pos = range_str.find('-');
            size_t start = 0;
            size_t end = file_size - 1;
            
            if (dash_pos != std::string::npos) {
                std::string start_str = range_str.substr(0, dash_pos);
                std::string end_str = range_str.substr(dash_pos + 1);
                
                if (!start_str.empty()) {
                    start = std::stoull(start_str);
                }
                if (!end_str.empty()) {
                    end = std::stoull(end_str);
                }
            }
			logger_->log(logger::level::info, "Range: " + std::to_string(start) + "-" + std::to_string(end));
            
            if (start >= file_size || end >= file_size || start > end) {
                res.status = 416;
                res.set_header("Content-Range", "bytes */" + std::to_string(file_size));
                return;
            }
            
            // 对于大文件使用分块下载
            if (file_size > buffer_chunk_size_) {
                handle_chunked_download(safe_path, req, res);
                return;
            }
            
            // 小文件直接读取范围
            std::string content;
            file_manager_->read_file_range(safe_path, start, end, content);
            res.status = 206;
            res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" + 
                          std::to_string(end) + "/" + std::to_string(file_size));
            res.set_header("Accept-Ranges", "bytes");
            res.set_content(content, content_type);
            return;
        }
        
        // 对于大文件使用分块下载
        if (file_size > buffer_chunk_size_) {
			if(cache_max_age_ != 0) {
				logger_->log(logger::level::info, "Cache enabled. Max age: " + std::to_string(cache_max_age_));
				res.set_header("Cache-Control", "public, max-age=" + std::to_string(cache_max_age_));
			}
            handle_chunked_download(safe_path, req, res);
            return;
        }
        
        // 小文件处理
        std::string content;
        file_manager_->read_file(safe_path, content);
        
        // Gzip压缩
        if (should_compress(safe_path, content.size(), accept_encoding)) {
            std::string compressed;
            if (compressor_->compress(content, compressed)) {
                res.set_content(compressed, content_type);
                res.set_header("Content-Encoding", "gzip");
				logger_->log(logger::level::info, "Gzip enabled. Compressed file size: " + std::to_string(compressed.size()));
            } else {
                res.set_content(content, content_type);
            }
        } else {
			logger_->log(logger::level::info, "Gzip disabled.");
            res.set_content(content, content_type);
        }
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "File request error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
    }
}

void http_server::handle_head_request(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        // 云盘特殊处理
        if(path.find("/cloud-drive") != std::string::npos && fs::is_directory(safe_path)) {
            path = "/cloud-drive.html";
            safe_path = file_manager_->sanitize_path(path);
        }

        if (file_manager_->is_directory(safe_path)) {
            std::string index_path = safe_path + "/index.html";
            if (file_manager_->file_exists(index_path)) {
                safe_path = index_path;
            } else {
                res.status = 404;
                return;
            }
        }
        
        if (!file_manager_->file_exists(safe_path)) {
            res.status = 404;
            return;
        }
        
        size_t file_size = file_manager_->get_file_size(safe_path);
        std::string content_type = file_manager_->get_content_type(safe_path);
        
        // 设置响应头
        res.set_header("Content-Type", content_type);
        res.set_header("Content-Length", std::to_string(file_size));
        res.set_header("Accept-Ranges", "bytes");
        
        // 处理Range请求头
        auto range_header = req.get_header_value("Range");
        if (!range_header.empty()) {
            std::string range_str = range_header.substr(6); // "bytes="
            size_t dash_pos = range_str.find('-');
            size_t start = 0;
            size_t end = file_size - 1;
            
            if (dash_pos != std::string::npos) {
                std::string start_str = range_str.substr(0, dash_pos);
                std::string end_str = range_str.substr(dash_pos + 1);
                
                if (!start_str.empty()) {
                    start = std::stoull(start_str);
                }
                if (!end_str.empty()) {
                    end = std::stoull(end_str);
                }
            }
            
            if (start >= file_size || end >= file_size || start > end) {
                res.status = 416;
                res.set_header("Content-Range", "bytes */" + std::to_string(file_size));
                return;
            }
            
            res.status = 206;
            res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" + 
                          std::to_string(end) + "/" + std::to_string(file_size));
            res.set_header("Content-Length", std::to_string(end - start + 1));
        }
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "HEAD request error: " + std::string(e.what()));
        res.status = 500;
    }
}

void http_server::handle_chunked_download(const std::string& path, const httplib::Request& req, httplib::Response& res) {
	logger_->log(logger::level::info, "Chunked downloading enabled.");
    try {
        size_t file_size = file_manager_->get_file_size(path);
        std::string content_type = file_manager_->get_content_type(path);
        
        // 解析Range头
        size_t start = 0;
        size_t end = file_size - 1;
        
        auto range_header = req.get_header_value("Range");
        if (!range_header.empty()) {
            std::string range_str = range_header.substr(6); // "bytes="
            size_t dash_pos = range_str.find('-');
            
            if (dash_pos != std::string::npos) {
                std::string start_str = range_str.substr(0, dash_pos);
                std::string end_str = range_str.substr(dash_pos + 1);
                
                if (!start_str.empty()) {
                    start = std::stoull(start_str);
                }
                if (!end_str.empty()) {
                    end = std::stoull(end_str);
                }
            }
        }
        
        // 设置响应头
        res.status = range_header.empty() ? 200 : 206;
        res.set_header("Content-Type", content_type);
        res.set_header("Accept-Ranges", "bytes");
        
        if (!range_header.empty()) {
            res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" + 
                          std::to_string(end) + "/" + std::to_string(file_size));
        }
        
        // 使用Content Provider分块发送内容
        size_t chunk_size = buffer_chunk_size_;
        size_t total_size = end - start + 1;
        
        res.set_content_provider(
            total_size,
            content_type.c_str(),
            [this, path, start, total_size, chunk_size](size_t offset, size_t length, httplib::DataSink &sink) {
				(void)length;
                size_t read_start = start + offset;
                size_t read_end = std::min(read_start + chunk_size - 1, start + total_size - 1);
                
                std::string chunk;
                if (file_manager_->read_file_range(path, read_start, read_end, chunk)) {
                    sink.write(chunk.data(), chunk.size());
                    return true;
                }
                return false;
            }
        );
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "Chunked download error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
    }
}

void http_server::handle_upload_request(const httplib::Request& req, httplib::Response& res) {
    try {
		std::string password = req.get_param_value("password");
		if(password != admin_password_) {
			res.status = 403;
			res.set_content("Password wrong", "text/plain");
			return;
		}
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        if (!file_manager_->create_directory(safe_path)) {
            file_manager_->create_directory(fs::path(safe_path).parent_path().string());
        }
        
        if (req.is_multipart_form_data()) {
            // 检查是否有文件上传
            if (req.form.has_file("file")) {
                const auto& file = req.form.get_file("file");
                if (!file.content.empty()) {
                    std::string file_path = safe_path + "/" + file.filename;
                    if (file_manager_->write_file(file_path, file.content)) {
                        res.set_content("Upload successful: " + file.filename, "text/plain");
                    } else {
                        res.status = 500;
                        res.set_content("Upload failed: " + file.filename, "text/plain");
                    }
                } else {
                    res.status = 400;
                    res.set_content("Empty file content", "text/plain");
                }
            } else {
                res.status = 400;
                res.set_content("No file provided in form data", "text/plain");
            }
        } else {
            // 处理非 multipart 的情况（原始请求体）
            if (!req.body.empty()) {
                // 从路径中提取文件名或使用默认名称
                std::string filename = "upload_" + std::to_string(std::time(nullptr));
                std::string file_path = safe_path + "/" + filename;
                if (file_manager_->write_file(file_path, req.body)) {
                    res.set_content("Upload successful: " + filename, "text/plain");
                } else {
                    res.status = 500;
                    res.set_content("Upload failed", "text/plain");
                }
            } else {
                res.status = 400;
                res.set_content("No file content provided", "text/plain");
            }
        }
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "Upload error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Upload failed: " + std::string(e.what()), "text/plain");
    }
}

bool http_server::handle_chunked_upload(const httplib::Request& req, httplib::Response& res) {
    try {
		std::string password = req.get_param_value("password");
		if(password != admin_password_) {
			res.status = 403;
			res.set_content("Password wrong", "text/plain");
			return false;
		}
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        // 获取文件名
        std::string filename;
        auto content_disposition = req.get_header_value("Content-Disposition");
        if (!content_disposition.empty()) {
            size_t name_pos = content_disposition.find("filename=");
            if (name_pos != std::string::npos) {
                filename = content_disposition.substr(name_pos + 9);
                if (filename.front() == '"' && filename.back() == '"') {
                    filename = filename.substr(1, filename.size() - 2);
                }
            }
        }
        
        if (filename.empty()) {
            filename = "upload_" + std::to_string(std::time(nullptr));
        }
        
        std::string file_path = safe_path + "/" + filename;
        
        // 创建目录
        file_manager_->create_directory(fs::path(file_path).parent_path().string());
        
        // 写入文件
        std::ofstream file(file_path, std::ios::binary);
        if (!file) {
            res.status = 500;
            res.set_content("Failed to create file", "text/plain");
            return true;
        }
        
        file.write(req.body.data(), req.body.size());
        file.close();
        
        res.set_content("Upload successful: " + filename, "text/plain");
        return true;
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "Chunked upload error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Upload failed: " + std::string(e.what()), "text/plain");
        return true;
    }
}

void http_server::handle_delete_request(const httplib::Request& req, httplib::Response& res) {
    try {
		std::string password = req.get_param_value("password");
		if(password != admin_password_) {
			res.status = 403;
			res.set_content("Password wrong", "text/plain");
			return;
		}
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        if (file_manager_->delete_file(safe_path)) {
            res.set_content("File deleted successfully", "text/plain");
        } else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "Delete error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Delete failed", "text/plain");
    }
}

void http_server::handle_mkdir_request(const httplib::Request& req, httplib::Response& res) {
    try {
		std::string password = req.get_param_value("password");
		if(password != admin_password_) {
			res.status = 403;
			res.set_content("Password wrong", "text/plain");
			return;
		}
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        if (file_manager_->create_directory(safe_path)) {
            res.set_content("Directory created successfully", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to create directory", "text/plain");
        }
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "Mkdir error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Failed to create directory", "text/plain");
    }
}

void http_server::handle_list_request(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string path = req.path;
        std::string safe_path = file_manager_->sanitize_path(path);
        
        if (!file_manager_->is_directory(safe_path)) {
            res.status = 404;
            res.set_content("Directory not found", "text/plain");
            return;
        }
        
        auto files = file_manager_->list_directory(safe_path);
        
        std::ostringstream oss;
        for (const auto& file : files) {
            oss << file.name << (file.is_directory ? "/" : "") << "\n";
        }
        
        res.set_content(oss.str(), "text/plain");
        
    } catch (const std::exception& e) {
        logger_->log(logger::level::error, "List error: " + std::string(e.what()));
        res.status = 500;
        res.set_content("Failed to list directory", "text/plain");
    }
}

std::string http_server::get_client_ip(const httplib::Request& req) const {
    auto x_forwarded_for = req.get_header_value("X-Forwarded-For");
    if (!x_forwarded_for.empty()) {
        return x_forwarded_for;
    }
    
    auto x_real_ip = req.get_header_value("X-Real-IP");
    if (!x_real_ip.empty()) {
        return x_real_ip;
    }
    
    return req.remote_addr;
}

bool http_server::should_compress(const std::string& path, size_t size, const std::string& accept_encoding) const {
    // 只对文本文件和小于缓冲区块大小的文件进行压缩
    static const std::vector<std::string> compressible_types = {
        "text/html", "text/css", "application/javascript", "application/json", 
        "application/xml", "text/plain", "image/svg+xml"
    };
    
    std::string content_type = file_manager_->get_content_type(path);
    bool is_compressible = std::find(compressible_types.begin(), 
                                   compressible_types.end(), 
                                   content_type) != compressible_types.end();
    
    return is_compressible && 
           size <= buffer_chunk_size_ && 
           compressor_->is_gzip_supported(accept_encoding);
}

} // namespace to_https_server
