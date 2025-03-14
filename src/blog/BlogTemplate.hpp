//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_BLOG_TEMPLATE_HPP
#define TINA_BLOG_BLOG_TEMPLATE_HPP

#include "Nginx.hpp"
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>

class BlogTemplate
{
public:
    std::string loadTemplate(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string renderTemplate(const std::string& templateContent,
                               const std::unordered_map<std::string, std::string&> variables)
    {
        std::string result = templateContent;
        for (const auto& [key, value] : variables)
        {
            std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            while ((pos = result.find(placeholder, pos)) != std::string::npos)
            {
                result.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
        return result;
    }
};


#endif //TINA_BLOG_BLOG_TEMPLATE_HPP
