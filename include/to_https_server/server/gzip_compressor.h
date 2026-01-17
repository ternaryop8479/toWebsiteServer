#ifndef TO_HTTPS_SERVER_GZIP_COMPRESSOR_H
#define TO_HTTPS_SERVER_GZIP_COMPRESSOR_H

#include <string>
#include <vector>

namespace to_https_server {

class gzip_compressor {
public:
    gzip_compressor();
    ~gzip_compressor();
    
    bool compress(const std::string& input, std::string& output);
    bool decompress(const std::string& input, std::string& output);
    
    bool is_gzip_supported(const std::string& accept_encoding) const;
    
private:
    static const int GZIP_WINDOW_BITS = 15 + 16;
    static const int GZIP_ENCODING = 16;
};

} // namespace to_https_server

#endif // TO_HTTPS_SERVER_GZIP_COMPRESSOR_H
