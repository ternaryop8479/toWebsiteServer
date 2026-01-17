#ifndef TO_HTTPS_SERVER_LOGGER_H
#define TO_HTTPS_SERVER_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace to_https_server {

class logger {
public:
    enum class level {
        debug,
        info,
        warning,
        error,
        critical
    };
    
    logger(const std::string& log_dir);
    ~logger();
    
    void log(level lvl, const std::string& message);
    void log(level lvl, const std::string& message, int client_id);
    
private:
	void update_log_file();

    std::string get_current_time() const;
    std::string level_to_string(level lvl) const;
    
    std::ofstream log_file_;
    std::mutex log_mutex_;
    std::string log_dir_;
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_LOGGER_H
