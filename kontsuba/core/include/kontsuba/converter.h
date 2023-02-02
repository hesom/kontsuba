#pragma once
#include <memory>
#include <string>

namespace Kontsuba {
    class Converter_Impl;

    class Converter {
    public:
        Converter(const std::string& inputFile, const std::string& outputDirectory);
        ~Converter();

    private:
        std::string m_inputFile;
        std::string m_outputDirectory;
        std::unique_ptr<Converter_Impl> m_impl;
    };
}