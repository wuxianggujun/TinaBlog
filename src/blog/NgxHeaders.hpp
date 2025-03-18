//
// Created by wuxianggujun on 2025/3/18.
//

#ifndef TINA_BLOG_NGX_HEADERS_HPP
#define TINA_BLOG_NGX_HEADERS_HPP

#include "NginxContext.hpp"

template <typename T, T ngx_http_request_t::* ptr>
class NgxHeaders final : public NginxContext<T>
{public:
   
};


#endif //TINA_BLOG_NGX_HEADERS_HPP
