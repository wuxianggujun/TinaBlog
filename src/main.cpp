//
// Created by wuxianggujun on 2025/3/29.
//

#include <iostream>
#include <string>

std::string getFrontendPath() {
    return TINA_BLOG_FRONTEND_PATH;
}

int main(){
    std::cout << "Hello, world!" << getFrontendPath() <<std::endl;
    return 0;
}