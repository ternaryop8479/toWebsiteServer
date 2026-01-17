#include <to_https_server/server/file_manager.h>
#include <to_https_server/utils/logger.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

namespace to_https_server {

file_manager::file_manager(const std::string& root_path, const std::string& trash_path)
	: root_path_(root_path), trash_path_(trash_path) {
	if(root_path_.find_last_of('/') == root_path_.size() - 1) {
		root_path_ = root_path_.substr(0, root_path_.size() - 1);
	}
	
	if (!fs::exists(root_path_)) {
		fs::create_directories(root_path_);
	}
	
	if (!fs::exists(trash_path_)) {
		fs::create_directories(trash_path_);
	}
}

bool file_manager::file_exists(const std::string& path) const {
	return fs::exists(get_safe_path(path));
}

bool file_manager::is_directory(const std::string& path) const {
	return fs::is_directory(get_safe_path(path));
}

size_t file_manager::get_file_size(const std::string& path) const {
	return fs::file_size(get_safe_path(path));
}

bool file_manager::read_file(const std::string& path, std::string& content) const {
	std::ifstream file(get_safe_path(path), std::ios::binary);
	if (!file) {
		return false;
	}
	
	content.assign((std::istreambuf_iterator<char>(file)),
				   std::istreambuf_iterator<char>());
	return true;
}

bool file_manager::read_file_range(const std::string& path, size_t start, size_t end,
								 std::string& content) const {
	std::ifstream file(get_safe_path(path), std::ios::binary);
	if (!file) {
		return false;
	}
	
	file.seekg(0, std::ios::end);
	size_t file_size = file.tellg();
	
	if (start >= file_size) {
		return false;
	}
	
	if (end >= file_size) {
		end = file_size - 1;
	}
	
	if (start > end) {
		return false;
	}
	
	size_t range_size = end - start + 1;
	content.resize(range_size);
	
	file.seekg(start);
	file.read(&content[0], range_size);
	
	return true;
}

bool file_manager::write_file(const std::string& path, const std::string& content) {
	fs::create_directories(fs::path(get_safe_path(path)).parent_path());
	
	std::ofstream file(get_safe_path(path), std::ios::binary);
	if (!file) {
		return false;
	}
	
	file.write(content.data(), content.size());
	return true;
}

bool file_manager::append_file(const std::string& path, const std::string& content) {
	fs::create_directories(fs::path(get_safe_path(path)).parent_path());
	
	std::ofstream file(get_safe_path(path), std::ios::binary | std::ios::app);
	if (!file) {
		return false;
	}
	
	file.write(content.data(), content.size());
	return true;
}

bool file_manager::delete_file(const std::string& path) {
	std::string safe_path = get_safe_path(path);
	if (!fs::exists(safe_path)) {
		return false;
	}
	
	std::string trash_filename = generate_trash_filename(fs::path(safe_path).filename().string());
	std::string trash_path = trash_path_ + "/" + trash_filename;
	
	try {
		fs::rename(safe_path, trash_path);
		return true;
	} catch (const fs::filesystem_error& e) {
		return false;
	}
}

bool file_manager::move_file(const std::string& src, const std::string& dest) {
	std::string safe_src = get_safe_path(src);
	std::string safe_dest = get_safe_path(dest);
	
	if (!fs::exists(safe_src)) {
		return false;
	}
	
	fs::create_directories(fs::path(safe_dest).parent_path());
	
	try {
		fs::rename(safe_src, safe_dest);
		return true;
	} catch (const fs::filesystem_error& e) {
		return false;
	}
}

bool file_manager::create_directory(const std::string& path) {
	return fs::create_directories(get_safe_path(path));
}

std::vector<file_info> file_manager::list_directory(const std::string& path) const {
	std::vector<file_info> files;
	
	std::string safe_path = get_safe_path(path);
	if (!fs::exists(safe_path) || !fs::is_directory(safe_path)) {
		return files;
	}
	
	for (const auto& entry : fs::directory_iterator(safe_path)) {
		file_info info;
		info.name = entry.path().filename().string();
		info.is_directory = entry.is_directory();
		
		if (entry.is_regular_file()) {
			info.size = entry.file_size();
		} else {
			info.size = 0;
		}
		
		auto last_write = entry.last_write_time();
		auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
			last_write - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
		
		std::time_t time = std::chrono::system_clock::to_time_t(sctp);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
		info.last_modified = ss.str();
		
		files.push_back(info);
	}
	
	return files;
}

std::string file_manager::get_content_type(const std::string& path) const {
	std::string extension = fs::path(path).extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	
	static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".txt", "text/plain"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/x-icon"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".pdf", "application/pdf"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".svg", "image/svg+xml"},
        {".webp", "image/webp"},
        {".mp4", "video/mp4"},
        {".avi", "video/x-msvideo"},
        {".mov", "video/quicktime"},
        {".mkv", "video/x-matroska"},
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".ogg", "audio/ogg"},
        {".flac", "audio/flac"},
        {".zip", "application/zip"},
        {".rar", "application/vnd.rar"},
        {".tar", "application/x-tar"},
        {".gz", "application/gzip"},
        {".7z", "application/x-7z-compressed"},
        {".font", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".eot", "application/vnd.ms-fontobject"},
        {".csv", "text/csv"},
        {".tsv", "text/tsv"},
        {".yaml", "application/yaml"},
        {".yml", "application/yaml"},
        {".sql", "text/plain"},
        {".log", "text/plain"},
        {".sh", "text/plain"},
        {".bat", "text/plain"},
        {".ps1", "text/plain"},
        {".py", "text/plain"},
        {".java", "text/plain"},
        {".cpp", "text/plain"},
        {".h", "text/plain"},
        {".c", "text/plain"},
        {".hpp", "text/plain"},
        {".cs", "text/plain"},
        {".php", "text/plain"},
        {".rb", "text/plain"},
        {".pl", "text/plain"},
        {".swift", "text/plain"},
        {".kt", "text/plain"},
        {".go", "text/plain"},
        {".rs", "text/plain"},
        {".scala", "text/plain"},
        {".lua", "text/plain"},
        {".groovy", "text/plain"},
        {".m", "text/plain"},
        {".mm", "text/plain"},
        {".ml", "text/plain"},
        {".mli", "text/plain"},
        {".r", "text/plain"},
        {".jl", "text/plain"},
        {".fs", "text/plain"},
        {".fsx", "text/plain"},
        {".fsi", "text/plain"},
        {".clj", "text/plain"},
        {".cljs", "text/plain"},
        {".cljc", "text/plain"},
        {".edn", "text/plain"},
        {".cljs.hl", "text/plain"},
        {".clj.hl", "text/plain"},
        {".cljc.hl", "text/plain"},
        {".edn.hl", "text/plain"},
        {".hs", "text/plain"},
        {".lhs", "text/plain"},
        {".lhs.hl", "text/plain"},
        {".hs.hl", "text/plain"},
        {".elm", "text/plain"},
        {".elm.hl", "text/plain"},
        {".erl", "text/plain"},
        {".erl.hl", "text/plain"},
        {".hrl", "text/plain"},
        {".hrl.hl", "text/plain"},
        {".ex", "text/plain"},
        {".exs", "text/plain"},
        {".ex.hl", "text/plain"},
        {".exs.hl", "text/plain"},
        {".eex", "text/plain"},
        {".eex.hl", "text/plain"},
        {".leex", "text/plain"},
        {".leex.hl", "text/plain"},
        {".heex", "text/plain"},
        {".heex.hl", "text/plain"},
        {".exs.hl", "text/plain"},
        {".eex.hl", "text/plain"},
        {".leex.hl", "text/plain"},
        {".heex.hl", "text/plain"},
        {".f90", "text/plain"},
        {".f95", "text/plain"},
        {".f03", "text/plain"},
        {".f08", "text/plain"},
        {".f", "text/plain"},
        {".for", "text/plain"},
        {".f77", "text/plain"},
        {".f90.hl", "text/plain"},
        {".f95.hl", "text/plain"},
        {".f03.hl", "text/plain"},
        {".f08.hl", "text/plain"},
        {".f.hl", "text/plain"},
        {".for.hl", "text/plain"},
        {".f77.hl", "text/plain"},
        {".pro", "text/plain"},
        {".pro.hl", "text/plain"}
	};
	
	auto it = mime_types.find(extension);
	if (it != mime_types.end()) {
		return it->second;
	}
	
	return "application/octet-stream";
}

std::string file_manager::sanitize_path(const std::string& path) const {
	return get_safe_path(path);
}

std::string file_manager::get_safe_path(const std::string& path) const {
	struct {
		// 规范化路径
		std::string normalize_path(const std::string& path) const {
			std::vector<std::string> parts;
			std::stringstream ss(path);
			std::string part;
			
			// 分割路径
			while (std::getline(ss, part, '/')) {
				if (part == "..") {
					if (!parts.empty()) {
						parts.pop_back(); // 向上移动一级
					}
					// 如果parts为空，忽略".."以防止跳出根目录
				} else if (part != "." && !part.empty()) {
					parts.push_back(part);
				}
			}
			
			// 重新构建路径
			std::string normalized;
			for (const auto& p : parts) {
				normalized += "/" + p;
			}
			
			return normalized.empty() ? "/" : normalized;
		}
	} inlineToolKit;
    // 检查路径是否以root_path开头且不包含危险模式
    if (path.compare(0, root_path_.length(), root_path_) == 0 &&
        path.find("/../") == std::string::npos &&
        path.find("//") == std::string::npos) {
        
        // 移除末尾的斜杠（如果有）
        if (!path.empty() && path.back() == '/') {
            return path.substr(0, path.size() - 1);
        }
        return path;
    }
    
    // 构建完整路径
    std::string result = root_path_ + path;
    
    // 规范化路径
    result = inlineToolKit.normalize_path(result);
    
    // 确保路径不以root_path外的目录结束
    if (result.find(root_path_) != 0) {
        // 如果不以root_path开头，则返回root_path
        result = root_path_;
    }
    
    // 移除末尾的斜杠（如果有）
    if (!result.empty() && result.back() == '/') {
        result = result.substr(0, result.size() - 1);
    }
    
    return result;
}

std::string file_manager::generate_trash_filename(const std::string& original_name) const {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	
	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S_");
	ss << original_name;
	
	return ss.str();
}

} // namespace to_https_server
