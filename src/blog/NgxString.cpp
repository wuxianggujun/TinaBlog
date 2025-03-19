#include "NgxString.hpp"
#include <algorithm>
#include "NgxPool.hpp"

NgxString::NgxString(NgxPool& pool) {
    ptr_ = static_cast<ngx_str_t*>(pool.alloc(sizeof(ngx_str_t)));
    if (ptr_) {
        ptr_->len = 0;
        ptr_->data = nullptr;
    }
}

NgxString::NgxString(const ngx_str_t& str, NgxPool& pool) {
    ptr_ = static_cast<ngx_str_t*>(pool.alloc(sizeof(ngx_str_t)));
    if (ptr_) {
        ptr_->data = static_cast<u_char*>(pool.nalloc(str.len));
        if (ptr_->data) {
            ngx_memcpy(ptr_->data, str.data, str.len);
            ptr_->len = str.len;
        } else {
            ptr_ = nullptr;
        }
    }
}

NgxString::NgxString(const u_char* data, size_t len, NgxPool& pool) {
    if (!data || len == 0) {
        ptr_ = nullptr;
        return;
    }
    
    ptr_ = static_cast<ngx_str_t*>(pool.alloc(sizeof(ngx_str_t)));
    if (ptr_) {
        ptr_->data = static_cast<u_char*>(pool.nalloc(len));
        if (ptr_->data) {
            ngx_memcpy(ptr_->data, data, len);
            ptr_->len = len;
        } else {
            ptr_ = nullptr;
        }
    }
}

void NgxString::set(const u_char* data, size_t len) {
    if (ptr_) {
        ptr_->data = const_cast<u_char*>(data);
        ptr_->len = len;
    }
}

int NgxString::compare(const char* str) const noexcept {
    if (!ptr_ || !str) {
        return ptr_ ? 1 : (str ? -1 : 0);
    }
    
    size_t str_len = strlen(str);
    int result = ngx_strncmp(ptr_->data, reinterpret_cast<const u_char*>(str), 
                            std::min(ptr_->len, str_len));
    
    if (result != 0) {
        return result;
    }
    
    return ptr_->len - str_len;
}

int NgxString::compare(const NgxString& other) const noexcept {
    if (!ptr_ || !other.ptr_) {
        return ptr_ ? 1 : (other.ptr_ ? -1 : 0);
    }
    
    int result = ngx_strncmp(ptr_->data, other.ptr_->data,
                            std::min(ptr_->len, other.ptr_->len));
    
    if (result != 0) {
        return result;
    }
    
    return ptr_->len - other.ptr_->len;
}

NgxString NgxString::create(const std::string& str, NgxPool& pool) {
    if (str.empty()) {
        return NgxString();
    }
    return NgxString(reinterpret_cast<const u_char*>(str.data()), str.length(), pool);
}

NgxString NgxString::create(const char* str, NgxPool& pool) {
    if (!str) {
        return NgxString();
    }
    size_t len = std::strlen(str);
    return NgxString(reinterpret_cast<const u_char*>(str), len, pool);
}

NgxString NgxString::create(const std::string_view sv, NgxPool& pool) {
    if (sv.empty()) {
        return NgxString();
    }
    return NgxString(reinterpret_cast<const u_char*>(sv.data()), sv.length(), pool);
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

bool NgxString::append(const NgxString& other, NgxPool& pool) {
    if (!other.valid()) return true;  // 空字符串，视为成功
    
    auto new_str = NgxString(nullptr, length() + other.length(), pool);
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
    auto temp = create(str, pool);
    return append(temp, pool);
}

bool NgxString::append(const std::string& str, NgxPool& pool) {
    if (str.empty()) return true;
    auto temp = create(str, pool);
    return append(temp, pool);
}

NgxString NgxString::substr(size_t pos, size_t len, NgxPool& pool) const {
    if (!valid() || pos >= length()) {
        return NgxString();
    }
    
    size_t actual_len = std::min(len, length() - pos);
    return NgxString(ptr_->data + pos, actual_len, pool);
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
