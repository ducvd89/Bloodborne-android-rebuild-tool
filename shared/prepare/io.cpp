// SPDX-License-Identifier: GPL-2.0-or-later
#include "io.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdlib>

MappedFile& MappedFile::operator=(MappedFile&& o) noexcept {
    close();
    data_ = o.data_; size_ = o.size_; map_ = o.map_; map_len_ = o.map_len_;
    fallback_ = std::move(o.fallback_);
    if (!fallback_.empty()) data_ = fallback_.data();
    o.data_ = nullptr; o.size_ = 0; o.map_ = nullptr; o.map_len_ = 0;
    return *this;
}

void MappedFile::close() {
    if (map_) munmap(map_, map_len_);
    map_ = nullptr;
    map_len_ = 0;
    data_ = nullptr;
    size_ = 0;
    Bytes().swap(fallback_);
}

MappedFile read_file(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) raise_errno(errno, path);
    struct stat st;
    if (fstat(fd, &st) < 0) { int e = errno; ::close(fd); raise_errno(e, path); }
    if (S_ISDIR(st.st_mode)) { ::close(fd); raise_errno(EISDIR, path); }
    MappedFile f;
    if (S_ISREG(st.st_mode) && st.st_size > 0) {
        void* m = mmap(nullptr, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            madvise(m, (size_t)st.st_size, MADV_SEQUENTIAL);
            f.map_ = m;
            f.map_len_ = f.size_ = (size_t)st.st_size;
            f.data_ = (const u8*)m;
            ::close(fd);
            return f;
        }
    }
    Bytes buf;
    u8 chunk[1 << 16];
    for (;;) {
        ssize_t n = ::read(fd, chunk, sizeof chunk);
        if (n < 0) { if (errno == EINTR) continue; int e = errno; ::close(fd); raise_errno(e, path); }
        if (!n) break;
        buf.insert(buf.end(), chunk, chunk + n);
    }
    ::close(fd);
    f.fallback_ = std::move(buf);
    f.data_ = f.fallback_.data();
    f.size_ = f.fallback_.size();
    return f;
}

OutFile::OutFile(const std::string& path) : path_(path) {
    fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd_ < 0) raise_errno(errno, path);
}
void OutFile::write(const void* p, size_t n) {
    const char* c = (const char*)p;
    while (n) {
        ssize_t w = ::write(fd_, c, n > (1u << 30) ? (1u << 30) : n);
        if (w < 0) { if (errno == EINTR) continue; raise_errno(errno, path_); }
        c += w;
        n -= (size_t)w;
    }
}
void OutFile::close_fd() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
}
void OutFile::close() {
    int fd = fd_;
    fd_ = -1;
    if (fd >= 0 && ::close(fd) < 0) raise_errno(errno, path_);
}

bool is_file(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void make_dir(const std::string& path) {
    if (mkdir(path.c_str(), 0777) < 0) raise_errno(errno, path);
}

void make_dirs(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) return;
        raise_errno(EEXIST, path);
    }
    size_t slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0) make_dirs(path.substr(0, slash));
    if (mkdir(path.c_str(), 0777) < 0 && errno != EEXIST) raise_errno(errno, path);
}

void rmtree_ignore_errors(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) < 0) return;
    if (S_ISDIR(st.st_mode)) {
        if (DIR* d = opendir(path.c_str())) {
            while (dirent* e = readdir(d)) {
                std::string n = e->d_name;
                if (n != "." && n != "..") rmtree_ignore_errors(path + "/" + n);
            }
            closedir(d);
        }
        rmdir(path.c_str());
    } else {
        unlink(path.c_str());
    }
}

void write_text_file(const std::string& path, const std::string& text) {
    OutFile f(path);
    f.write(text);
    f.close();
}

std::string resolve_path(const std::string& path) {
    char buf[PATH_MAX];
    std::string n = path_norm(path);
    if (realpath(n.c_str(), buf)) return buf;
    if (!n.empty() && n[0] == '/') return n;
    if (getcwd(buf, sizeof buf)) return n == "." ? std::string(buf) : std::string(buf) + "/" + n;
    return n;
}
