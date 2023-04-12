#pragma once
#include <memory>
#include <string>

enum importFlags {
    konSwitchHandedness = 1 << 0,
    konSwitchUV         = 1 << 1,
    konForceTwoSided    = 1 << 2
};

namespace Kontsuba {

void convert(const std::string &inputFile, const std::string &outputDirectory, const unsigned int flags);

}