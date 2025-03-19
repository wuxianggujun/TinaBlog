#include "NgxPool.hpp"
#include "NgxString.hpp"


NgxString NgxPool::create_string(const char* str) {
    if (!str || !ptr_) {
        return NgxString();
    }
    
    size_t len = std::strlen(str);
    if (len == 0) {
        return NgxString();
    }
    
    return NgxString::create(str, *this);
}

NgxString NgxPool::create_string(const std::string& str) {
    if (str.empty() || !ptr_) {
        return NgxString();
    }
    
    return NgxString::create(str,  *this);
}

NgxString NgxPool::create_string(const std::string_view sv) {
    if (sv.empty() || !ptr_) {
        return NgxString();
    }
    
    return NgxString::create(sv, *this);
}

NgxString NgxPool::create_string(const u_char* data, size_t len) {
    if (!data || len == 0 || !ptr_) {
        return NgxString();
    }
    
    return NgxString(data, len, *this);
}