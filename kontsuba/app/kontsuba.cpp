#include <iostream>
#include <string>

#include "args.hpp"
#include <kontsuba/converter.h>

int main(int argc, char const *argv[]) {
  args::ArgumentParser parser("Kontsuba - A 3D model converter");
  args::Group required(parser,
                       "Required arguments:", args::Group::Validators::All);
  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
  args::Positional<std::string> input(required, "input", "Input file");
  args::Positional<std::string> output(required, "output", "Output directory");
  args::CompletionFlag completion(parser, {"complete"});
  args::Group optional(parser,
                       "Optional arguments:", args::Group::Validators::DontCare);
  args::Flag switchHandedness(optional, "switchHandedness", "Switch handedness of imported scene", {"hand", "handed", "handedness"});
  args::Flag switchUV(optional, "switchUV", "Switch UV coordinate origin to top left", {"uv"});

  try {
    parser.ParseCLI(argc, argv);
  } catch (const args::Completion &e) {
    std::cout << e.what();
    return 0;
  } catch (const args::Help &) {
    std::cout << parser;
    return 0;
  } catch (const args::ParseError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  } catch (const args::ValidationError &e) {
    std::cerr << "Missing arguments" << std::endl;
    std::cerr << parser;
    return 1;
  }

  unsigned int flags = 0;
  if (args::get(switchHandedness)) {
      flags |= 0x1;
  }
  if (args::get(switchUV)) {
      flags |= 0x2;
  }

  const std::string path = args::get(input);
  const std::string outputDir = args::get(output);

  Kontsuba::convert(path, outputDir, flags);

  return 0;
}
