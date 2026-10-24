// SPDX-License-Identifier: GPL-2.0-or-later
// File helpers; failures raise Python-style OSError subclasses with Python's message format.
#pragma once
#include <string>

#include "pyish.h"

class MappedFile {
public:
    MappedFile() = default;
    MappedFile(MappedFile&& o) noexcept { *this = std::move(o); }
    MappedFile& operator=(MappedFile&& o) noexcept;
    MappedFile(const MappedFile&) = delete;
    ~MappedFile() { close(); }
    View view() const { return {data_, size_}; }
    void close();

private:
    friend MappedFile read_file(const std::string& path);
    const u8* data_ = nullptr;
    size_t size_ = 0;
    void* map_ = nullptr;
    size_t map_len_ = 0;
    Bytes fallback_;
};
MappedFile read_file(const std::string& path);  // Path.read_bytes()

class OutFile {
public:
    explicit OutFile(const std::string& path);  // open(path, 'wb')
    ~OutFile() { close_fd(); }
    void write(const void* p, size_t n);
    void write(const std::string& s) { write(s.data(), s.size()); }
    void close();

private:
    void close_fd();
    int fd_ = -1;
    std::string path_;
};

bool is_file(const std::string& path);  // Path.is_file()
void make_dirs(const std::string& path);  // mkdir(parents=True, exist_ok=True)
void make_dir(const std::string& path);   // mkdir()
void rmtree_ignore_errors(const std::string& path);
void write_text_file(const std::string& path, const std::string& text);  // Path.write_text()
std::string resolve_path(const std::string& path);  // Path.resolve()
