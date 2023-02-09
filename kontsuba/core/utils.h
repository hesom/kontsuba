#pragma once

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

#ifdef __unix__
inline fs::path expand(const fs::path &path){
    if(path.empty()) return path;

    const char * home = getenv("HOME");
    if(home == nullptr){
        throw std::runtime_error("HOME environment variable not set");
    }

    std::string s = path.c_str();
    if(s[0] == '~'){
        s = std::string(home) + s.substr(1, s.size() - 1);
        return fs::path(s);
    }else{
        return path;
    }
}
#else
inline fs::path expand(const fs::path & path){
    return path;
}
#endif