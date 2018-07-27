#include <boost/filesystem.hpp>

#include "campaign/campaign.h"

#include <gtest/gtest.h>

boost::filesystem::path test_data_dir;

TEST(campaign, Campaigns_from_json) {
  auto json = Utils::parseJSONFile(test_data_dir / "campaigns_sample.json");

  auto campaigns = campaign::campaignsFromJson(json);
  EXPECT_EQ(campaigns.size(), 1);

  EXPECT_EQ(campaigns.at(0).name, "campaign2");
  EXPECT_EQ(campaigns.at(0).install_message, "this is my message to show on the device");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  test_data_dir = argv[1];

  return RUN_ALL_TESTS();
}
#endif
