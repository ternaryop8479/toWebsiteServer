#ifndef TO_HTTPS_SERVER_FILE_MANAGER_H
#define TO_HTTPS_SERVER_FILE_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace to_https_server {

struct file_info {
    std::string name;
    bool is_directory;
    size_t size;
    std::string last_modified;
};

class file_manager {
public:
    file_manager(const std::string& root_path, const std::string& trash_path);
    
    bool file_exists(const std::string& path) const;
    bool is_directory(const std::string& path) const;
    size_t get_file_size(const std::string& path) const;
    
    bool read_file(const std::string& path, std::string& content) const;
    bool read_file_range(const std::string& path, size_t start, size_t end, 
                        std::string& content) const;
    
    bool write_file(const std::string& path, const std::string& content);
    bool append_file(const std::string& path, const std::string& content);
    
    bool delete_file(const std::string& path);
    bool move_file(const std::string& src, const std::string& dest);
    bool create_directory(const std::string& path);
    
    std::vector<file_info> list_directory(const std::string& path) const;
    
    std::string get_content_type(const std::string& path) const;
    
    std::string sanitize_path(const std::string& path) const;
    
private:
    std::string root_path_;
    std::string trash_path_;
    
    std::string get_safe_path(const std::string& path) const;
    std::string generate_trash_filename(const std::string& original_name) const;
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_FILE_MANAGER_H
