#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <kontsuba/converter.h>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(kontsuba_ext, m) {
  m.def(
      "convert",
      [](const std::string &inputFile, const std::string &outputDirectory) {
        Kontsuba::convert(inputFile, outputDirectory);
      },
      "inputFile"_a, "outputDirectory"_a);
}