#if defined(_WIN32) // Windows系操作系统，采用Win32 API
#include "TFMHeaders/TFM_WIN32.h"

#elif defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include "TFMHeaders/TFM_POSIX.h" // 支持POSIX API的Unix及类Unix系统

#endif
