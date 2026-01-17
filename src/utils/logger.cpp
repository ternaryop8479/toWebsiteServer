#include <to_https_server/utils/logger.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace to_https_server {

static std::string current_time_ = "";
static auto current_clock_ = std::chrono::high_resolution_clock::now();

logger::logger(const std::string& log_dir) : log_dir_(log_dir) {
	update_log_file();
}

logger::~logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void logger::update_log_file() {
    if (!fs::exists(log_dir_)) {
        fs::create_directories(log_dir_);
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
	if(current_time_ == ss.str()) {
		return;
	}
    current_time_ = ss.str();
    std::string filename = log_dir_ + "/" + ss.str() + ".log";
	if(log_file_.is_open()) {
		log_file_.close();
	}
    log_file_.open(filename, std::ios::app);
}

std::string logger::get_current_time() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string logger::level_to_string(level lvl) const {
    switch (lvl) {
        case level::debug: return "DEBUG";
        case level::info: return "INFO";
        case level::warning: return "WARNING";
        case level::error: return "ERROR";
        case level::critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

void logger::log(level lvl, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
	// 每五秒检查一次日志
	if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - current_clock_).count() >= 5) {
		update_log_file();
	}
    if (log_file_.is_open()) {
        log_file_ << "[" << get_current_time() << "] [" 
                  << level_to_string(lvl) << "] " << message << std::endl;
        log_file_.flush();
    }
}

// void logger::log(level lvl, const std::string& message, int client_id) {
//     std::lock_guard<std::mutex> lock(log_mutex_);
//     if (log_file_.is_open()) {
//         log_file_ << "[" << get_current_time() << "] [" 
//                   << level_to_string(lvl) << "] [Client " << client_id << "] " 
//                   << message << std::endl;
//         log_file_.flush();
//     }
// }

} // namespace to_https_server
