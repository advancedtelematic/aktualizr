
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, (const char *const *)argv, desc), vm);

    if (vm.count("help")) {
      std::cout << desc;
      return EXIT_SUCCESS;
    }

    po::notify(vm);
  } catch (const po::error &o) {
    std::cout << o.what();
    std::cout << desc;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
