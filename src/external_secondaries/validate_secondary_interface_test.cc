#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <regex>

namespace bpo = boost::program_options;

std::string firmware;
std::string target;

std::string exec(const std::string &cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr) result += buffer.data();
  }
  return result;
}

bool isVersionGood(const std::string &output) {
  std::string version = "1";
  return (output == version || output == version + "\n");
}

TEST(ECUInterface, check_version) {
  std::string cmd = target + " api-version";
  EXPECT_TRUE(isVersionGood(exec(cmd)));
}

TEST(ECUInterface, check_loglevel) {
  std::string cmd = target + " api-version --loglevel 4";
  EXPECT_TRUE(isVersionGood(exec(cmd)));
}

bool isECUListValid(const std::string &output) {
  std::smatch ecu_match;
  std::regex ecu_regex("\\w+(?: \\w+)?\\n?");

  unsigned int matched_symbols = 0;
  std::string::const_iterator search_start(output.cbegin());
  while (std::regex_search(search_start, output.cend(), ecu_match, ecu_regex)) {
    matched_symbols += ecu_match.length();
    search_start += ecu_match.position() + ecu_match.length();
  }
  return matched_symbols == output.size();
}

TEST(ECUInterface, check_list_ecus) {
  std::string cmd = target + " list-ecus";
  EXPECT_TRUE(isECUListValid(exec(cmd)));
}

TEST(ECUInterface, check_list_ecus_loglevel) {
  std::string cmd = target + " list-ecus --loglevel 4";
  EXPECT_TRUE(isECUListValid(exec(cmd)));
}

TEST(ECUInterface, install_good) {
  std::smatch ecu_match;
  std::regex ecu_regex("(\\w+) ?(\\w+)?\\n?");
  std::string output = exec(target + " list-ecus");

  EXPECT_TRUE(std::regex_search(output, ecu_match, ecu_regex));
  std::string cmd =
      target + " install-software  --firmware " + firmware + " --hardware-identifier " + ecu_match[1].str();

  if (ecu_match.size() == 3) {
    cmd += std::string(" --ecu-identifier ") + ecu_match[2].str();
  }
  EXPECT_EQ(system(cmd.c_str()), 0);
}

TEST(ECUInterface, install_bad) {
  std::string cmd = target +
                    " install-software --firmware somepath --hardware-identifier unexistent"
                    " --ecu-identifier unexistentserial";
  EXPECT_NE(system(cmd.c_str()), 0);
}

TEST(ECUInterface, install_bad_good_firmware) {
  std::string cmd = target + " install-software --firmware " + firmware +
                    " --hardware-identifier unexistent"
                    " --ecu-identifier unexistentserial";
  EXPECT_NE(system(cmd.c_str()), 0);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  bpo::options_description description("aktualizr-validate-secondary-interface command line options");
  bpo::variables_map vm;
  // clang-format off
  description.add_options()
    ("target", bpo::value<std::string>(&target)->required(), "target executable")
    ("firmware", bpo::value<std::string>(&firmware)->required(), "firmware to install");
  // clang-format on
  bpo::basic_parsed_options<char> parsed_options = bpo::command_line_parser(argc, argv).options(description).run();
  try {
    bpo::store(parsed_options, vm);
    bpo::notify(vm);
  } catch (const bpo::required_option &ex) {
    std::cout << ex.what() << std::endl << description;
    return EXIT_FAILURE;
  } catch (const bpo::error &ex) {
    std::cout << ex.what() << std::endl << description;
    return EXIT_FAILURE;
  }

  return RUN_ALL_TESTS();
}
#endif
