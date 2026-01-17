#ifndef TFM_WIN32API_H
#define TFM_WIN32API_H

#include <windows.h>
#include <string>

class toFileMemory {
private:
    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    HANDLE m_hMapping = NULL;
    LPVOID m_lpView = NULL;
    size_t m_size = 0;
    bool m_isOpen = false;

    void cleanup() {
        if (m_lpView) UnmapViewOfFile(m_lpView);
        if (m_hMapping) CloseHandle(m_hMapping);
        if (m_hFile != INVALID_HANDLE_VALUE) CloseHandle(m_hFile);
        m_lpView = nullptr;
        m_hMapping = NULL;
        m_hFile = INVALID_HANDLE_VALUE;
        m_size = 0;
        m_isOpen = false;
    }

public:
    toFileMemory() = default;

    ~toFileMemory() {
        save_close();
    }

    void* open(const std::string& fileName, size_t targetSize) {
        if (m_isOpen) save_close();
        if (targetSize == 0) return nullptr;

        // 打开文件（支持自动创建）
        HANDLE hFile = CreateFileA(
            fileName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,  // 存在则打开，不存在则创建
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) return nullptr;

        // 调整文件大小
        LARGE_INTEGER liSize;
        liSize.QuadPart = targetSize;
        
        if (GetFileSizeEx(hFile, &liSize) && liSize.QuadPart < targetSize) {
            liSize.QuadPart = targetSize;
            if (!SetFilePointerEx(hFile, liSize, NULL, FILE_BEGIN) ||
                !SetEndOfFile(hFile)) {
                CloseHandle(hFile);
                return nullptr;
            }
        }

        // 创建内存映射
        HANDLE hMapping = CreateFileMappingA(
            hFile,
            NULL,
            PAGE_READWRITE,
            liSize.HighPart,
            liSize.LowPart,
            NULL
        );

        if (!hMapping) {
            CloseHandle(hFile);
            return nullptr;
        }

        // 映射视图
        LPVOID lpView = MapViewOfFile(
            hMapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            targetSize
        );

        if (!lpView) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return nullptr;
        }

        // 更新成员变量
        m_hFile = hFile;
        m_hMapping = hMapping;
        m_lpView = lpView;
        m_size = targetSize;
        m_isOpen = true;

        return lpView;
    }

    void save() {
        if (m_isOpen) {
            FlushViewOfFile(m_lpView, m_size);
            FlushFileBuffers(m_hFile);  // 确保数据落盘
        }
    }

    void save_close() {
        if (m_isOpen) {
            save();
            cleanup();
        }
    }

    void unsave_close() {
        if (m_isOpen) cleanup();
    }

    toFileMemory(const toFileMemory&) = delete;
    toFileMemory& operator=(const toFileMemory&) = delete;
};

#endif
