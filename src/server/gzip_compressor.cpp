#include <to_https_server/server/gzip_compressor.h>
#include <zlib.h>
#include <cstring>

namespace to_https_server {

gzip_compressor::gzip_compressor() = default;
gzip_compressor::~gzip_compressor() = default;

bool gzip_compressor::compress(const std::string& input, std::string& output) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                     GZIP_WINDOW_BITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    
    zs.next_in = (Bytef*)input.data();
    zs.avail_in = input.size();
    
    int ret;
    char outbuffer[32768];
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (zs.total_out > output.size()) {
            output.append(outbuffer, zs.total_out - output.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    return ret == Z_STREAM_END;
}

bool gzip_compressor::decompress(const std::string& input, std::string& output) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit2(&zs, GZIP_WINDOW_BITS) != Z_OK) {
        return false;
    }
    
    zs.next_in = (Bytef*)input.data();
    zs.avail_in = input.size();
    
    int ret;
    char outbuffer[32768];
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = inflate(&zs, 0);
        
        if (zs.total_out > output.size()) {
            output.append(outbuffer, zs.total_out - output.size());
        }
        
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    return ret == Z_STREAM_END;
}

bool gzip_compressor::is_gzip_supported(const std::string& accept_encoding) const {
    return accept_encoding.find("gzip") != std::string::npos ||
           accept_encoding.find("deflate") != std::string::npos;
}

} // namespace to_https_server
