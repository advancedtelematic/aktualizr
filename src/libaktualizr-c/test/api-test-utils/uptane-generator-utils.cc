#include <boost/process.hpp>
#include "test_utils.h"

int main(int argc, char** argv) {
  if (argc == 4) {
    // Setup mode

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* generatorPath = argv[1];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    boost::filesystem::path metaDirPath(argv[2]);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* firmwarePath = argv[3];

    if (boost::filesystem::exists(metaDirPath)) {
      boost::filesystem::remove_all(metaDirPath);
    }

    Utils::createDirectories(metaDirPath, S_IRWXU);

    Process uptane_gen(generatorPath);

    uptane_gen.run({"generate", "--path", metaDirPath.string(), "--correlationid", "abc123"});
    uptane_gen.run({"image", "--path", metaDirPath.string(), "--filename", firmwarePath, "--targetname", "firmware.txt",
                    "--hwid", "primary_hw"});
    uptane_gen.run({"addtarget", "--path", metaDirPath.string(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                    "--serial", "CA:FE:A6:D2:84:9D"});
    uptane_gen.run({"signtargets", "--path", metaDirPath.string()});

    return (uptane_gen.lastExitCode() == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  if (argc == 2) {
    // Cleanup mode

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    boost::filesystem::path metaDirPath(argv[1]);

    if (!boost::filesystem::exists(metaDirPath)) {
      return EXIT_FAILURE;
    }

    boost::filesystem::remove_all(metaDirPath);
    return EXIT_SUCCESS;
  }

  fprintf(
      stderr,
      "Incorrect input params\nFor setup:\n\t%s UPTANE_GENERATOR_PATH META_DIR_PATH FIRMWARE_PATH\nFor cleanup:\n\t%s "
      "META_DIR_PATH",
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      argv[0], argv[0]);
  return EXIT_FAILURE;
}
