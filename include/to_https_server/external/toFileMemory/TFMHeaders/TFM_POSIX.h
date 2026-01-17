#ifndef TFM_POSIXAPI_H
#define TFM_POSIXAPI_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>

class toFileMemory {
private:
    int m_fd = -1;
    void* m_ptr = MAP_FAILED;
    size_t m_size = 0;
    bool m_isOpen = false;

public:
    toFileMemory() = default;

    ~toFileMemory() {
        save_close();
    }

    void* open(const std::string& fileName, size_t targetSize) {
        if (m_isOpen) save_close();

        // 处理特殊情况 targetSize=0
        if (targetSize == 0) return nullptr;

        // 打开或创建文件
        int fd = ::open(fileName.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd == -1) return nullptr;

        // 获取文件状态
        struct stat st;
        if (fstat(fd, &st) == -1) {
            ::close(fd);
            return nullptr;
        }

        // 调整文件大小
        if (st.st_size < static_cast<off_t>(targetSize)) {
            if (ftruncate(fd, targetSize) == -1) {
                ::close(fd);
                return nullptr;
            }
        }

        // 内存映射
        void* ptr = mmap(nullptr, targetSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            return nullptr;
        }

        m_fd = fd;
        m_ptr = ptr;
        m_size = targetSize;
        m_isOpen = true;
        return ptr;
    }

    void save() {
        if (m_isOpen) {
            msync(m_ptr, m_size, MS_SYNC);
        }
    }

    void save_close() {
        if (m_isOpen) {
            save();
            munmap(m_ptr, m_size);
            ::close(m_fd);
            m_ptr = MAP_FAILED;
            m_fd = -1;
            m_size = 0;
            m_isOpen = false;
        }
    }

    void unsave_close() {
        if (m_isOpen) {
            munmap(m_ptr, m_size);
            ::close(m_fd);
            m_ptr = MAP_FAILED;
            m_fd = -1;
            m_size = 0;
            m_isOpen = false;
        }
    }

    // 禁止拷贝和赋值
    toFileMemory(const toFileMemory&) = delete;
    toFileMemory& operator=(const toFileMemory&) = delete;
};

#endif
