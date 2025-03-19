#include "NgxString.hpp"

#include <algorithm>

#include "NgxPool.hpp"

    
NgxString NgxString::create(const std::string& str, ngx_pool_t* pool) {
    if (str.empty() || !pool) {
        return NgxString();
    }
    
    ngx_str_t* ngx_str = allocate(pool, str.length());
    if (!ngx_str) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, str.data(), str.length());
    return NgxString(ngx_str);
}

NgxString NgxString::create(const char* str, ngx_pool_t* pool) {
    if (!str || !pool) {
        return NgxString();
    }
    
    size_t len = std::strlen(str);
    ngx_str_t* ngx_str = allocate(pool, len);
    if (!ngx_str) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, str, len);
    return NgxString(ngx_str);
}

NgxString NgxString::create(std::string_view sv, ngx_pool_t* pool) {
    if (sv.empty() || !pool) {
        return NgxString();
    }
    
    ngx_str_t* ngx_str = allocate(pool, sv.length());
    if (!ngx_str) {
        return NgxString();
    }
    
    ngx_memcpy(ngx_str->data, sv.data(), sv.length());
    return NgxString(ngx_str);
}

bool NgxString::operator==(const NgxString& other) const noexcept {
    if (!ptr_ || !other.ptr_) {
        return ptr_ == other.ptr_;
    }
    
    if (ptr_->len != other.ptr_->len) {
        return false;
    }
    
    return ngx_memcmp(ptr_->data, other.ptr_->data, ptr_->len) == 0;
}

bool NgxString::operator!=(const NgxString& other) const noexcept {
    return !(*this == other);
}

ngx_str_t* NgxString::allocate(ngx_pool_t* pool, size_t len) {
    if (!pool || len == 0) {
        return nullptr;
    }
    
    // 分配ngx_str_t结构体
    ngx_str_t* str = static_cast<ngx_str_t*>(ngx_pcalloc(pool, sizeof(ngx_str_t)));
    if (!str) {
        return nullptr;
    }
    
    // 分配数据缓冲区
    str->data = static_cast<u_char*>(ngx_pnalloc(pool, len));
    if (!str->data) {
        return nullptr;
    }
    
    str->len = len;
    return str;
}

bool NgxString::append(const NgxString& other, NgxPool& pool) {
    if (!other.valid()) return true;  // 空字符串，视为成功
    
    auto new_str = pool.create_string(
        reinterpret_cast<const u_char*>(view().data()),
        length() + other.length()
    );
    
    if (!new_str.valid()) return false;
    
    if (valid()) {
        ngx_memcpy(new_str.get()->data, ptr_->data, ptr_->len);
        ngx_memcpy(new_str.get()->data + ptr_->len, other.ptr_->data, other.ptr_->len);
    } else {
        ngx_memcpy(new_str.get()->data, other.ptr_->data, other.ptr_->len);
    }
    
    *this = std::move(new_str);
    return true;
}

bool NgxString::append(const char* str, NgxPool& pool) {
    if (!str) return true;
    auto temp = pool.create_string(str);
    return append(temp, pool);
}

bool NgxString::append(const std::string& str, NgxPool& pool) {
    if (str.empty()) return true;
    auto temp = pool.create_string(str);
    return append(temp, pool);
}

NgxString NgxString::substr(size_t pos, size_t len, NgxPool& pool) const {
    if (!valid() || pos >= length()) {
        return NgxString();
    }
    
    size_t actual_len = std::min(len, length() - pos);
    return pool.create_string(ptr_->data + pos, actual_len);
}

size_t NgxString::find(const NgxString& str, size_t pos) const noexcept {
    if (!valid() || !str.valid() || pos >= length()) {
        return std::string::npos;
    }
    
    const char* haystack = reinterpret_cast<const char*>(ptr_->data + pos);
    const char* needle = reinterpret_cast<const char*>(str.ptr_->data);
    size_t haystack_len = ptr_->len - pos;
    size_t needle_len = str.ptr_->len;
    
    const char* found = std::search(
        haystack, haystack + haystack_len,
        needle, needle + needle_len
    );
    
    return found == haystack + haystack_len ? 
        std::string::npos : (found - reinterpret_cast<const char*>(ptr_->data));
}

size_t NgxString::find(const char* str, size_t pos) const noexcept {
    if (!valid() || !str || pos >= length()) {
        return std::string::npos;
    }
    
    const char* haystack = reinterpret_cast<const char*>(ptr_->data + pos);
    size_t haystack_len = ptr_->len - pos;
    size_t needle_len = std::strlen(str);
    
    const char* found = std::search(
        haystack, haystack + haystack_len,
        str, str + needle_len
    );
    
    return found == haystack + haystack_len ? 
        std::string::npos : (found - reinterpret_cast<const char*>(ptr_->data));
}
