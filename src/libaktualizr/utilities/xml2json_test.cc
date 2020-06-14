#include <gtest/gtest.h>
#include <sstream>

#include <libaktualizr/utils.h>
#include "utilities/xml2json.h"

TEST(xml2json, simple) {
  {
    std::stringstream inxml("<a/>");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":null})");
  }
  {
    std::stringstream inxml(R"(<a b="xxx"/>)");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"@b":"xxx"}})");
  }
  {
    std::stringstream inxml(R"(<a b="xxx" c="rrr"></a>)");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"@b":"xxx","@c":"rrr"}})");
  }
  {
    std::stringstream inxml("<a>xxx</a>");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":"xxx"})");
  }
  {
    std::stringstream inxml("<a><b>xxx</b></a>");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"b":"xxx"}})");
  }
  {
    std::stringstream inxml("<a><b>xxx</b><c>yyy</c></a>");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"b":"xxx","c":"yyy"}})");
  }
  {
    std::stringstream inxml(R"(<a xxx="1">yy</a>)");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"#text":"yy","@xxx":"1"}})");
  }
  {
    std::stringstream inxml(R"(<a><b>1</b><c>xx</c><b>2</b></a>)");
    auto j = xml2json::xml2json(inxml);
    EXPECT_EQ(Utils::jsonToCanonicalStr(j), R"({"a":{"b":["1","2"],"c":"xx"}})");
  }
}

static const std::string example_manifest = R"(
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote fetch="https://github.com" name="github" pushurl="ssh://git@github.com"/>
  <remote fetch="git://git.openembedded.org/" name="openembedded"/>
  <remote fetch="https://git.yoctoproject.org/git/" name="yocto"/>
  
  <default remote="github" revision="master"/>
  
  <project name="advancedtelematic/meta-updater" path="meta-updater" revision="6e1c9cf5cc59437ce07f5aec2dc62d665d218bdb" upstream="master"/>
  <project name="advancedtelematic/meta-updater-minnowboard" path="meta-updater-minnowboard" revision="c822d05f860c3a2437236696b22ef7536c0a1311" upstream="master"/>
  <project name="advancedtelematic/meta-updater-qemux86-64" path="meta-updater-qemux86-64" revision="162d1378659343a3ad34569c1315babe7246ec86" upstream="master"/>
  <project name="advancedtelematic/meta-updater-raspberrypi" path="meta-updater-raspberrypi" revision="501156e6d12e3207a5acb611984dce1856a7729c" upstream="master"/>
  <project name="meta-intel" remote="yocto" revision="eacd8eb9f762c90cec2825736e8c4d483966c4d4" upstream="master"/>
  <project name="meta-openembedded" remote="openembedded" revision="18506b797bcfe162999223b79919e7c730875bb4" upstream="master"/>
  <project name="meta-raspberrypi" remote="yocto" revision="254c9366b9c3309db6dc07beb80aba55e0c87f94" upstream="master"/>
  <project name="poky" remote="yocto" revision="3a751d5564fc6ee9aef225653cc7b8630fd25a35" upstream="master"/>
  <project name="ricardosalveti/meta-updater-riscv" path="meta-updater-riscv" revision="8164a21c04a7de91f90ada763104063540a84961" upstream="master"/>
  <project name="riscv/meta-riscv" path="meta-riscv" revision="0ba537b9270046b1c08d2b2f1cc9a9ca96ea0328" upstream="master"/>
</manifest>
)";

static const std::string example_json =
    R"({"manifest":{"default":{"@remote":"github","@revision":"master"},"project":[{"@name":"advancedtelematic/meta-updater","@path":"meta-updater","@revision":"6e1c9cf5cc59437ce07f5aec2dc62d665d218bdb","@upstream":"master"},{"@name":"advancedtelematic/meta-updater-minnowboard","@path":"meta-updater-minnowboard","@revision":"c822d05f860c3a2437236696b22ef7536c0a1311","@upstream":"master"},{"@name":"advancedtelematic/meta-updater-qemux86-64","@path":"meta-updater-qemux86-64","@revision":"162d1378659343a3ad34569c1315babe7246ec86","@upstream":"master"},{"@name":"advancedtelematic/meta-updater-raspberrypi","@path":"meta-updater-raspberrypi","@revision":"501156e6d12e3207a5acb611984dce1856a7729c","@upstream":"master"},{"@name":"meta-intel","@remote":"yocto","@revision":"eacd8eb9f762c90cec2825736e8c4d483966c4d4","@upstream":"master"},{"@name":"meta-openembedded","@remote":"openembedded","@revision":"18506b797bcfe162999223b79919e7c730875bb4","@upstream":"master"},{"@name":"meta-raspberrypi","@remote":"yocto","@revision":"254c9366b9c3309db6dc07beb80aba55e0c87f94","@upstream":"master"},{"@name":"poky","@remote":"yocto","@revision":"3a751d5564fc6ee9aef225653cc7b8630fd25a35","@upstream":"master"},{"@name":"ricardosalveti/meta-updater-riscv","@path":"meta-updater-riscv","@revision":"8164a21c04a7de91f90ada763104063540a84961","@upstream":"master"},{"@name":"riscv/meta-riscv","@path":"meta-riscv","@revision":"0ba537b9270046b1c08d2b2f1cc9a9ca96ea0328","@upstream":"master"}],"remote":[{"@fetch":"https://github.com","@name":"github","@pushurl":"ssh://git@github.com"},{"@fetch":"git://git.openembedded.org/","@name":"openembedded"},{"@fetch":"https://git.yoctoproject.org/git/","@name":"yocto"}]}})";

TEST(xml2json, manifest) {
  std::stringstream inxml(example_manifest);
  auto j = xml2json::xml2json(inxml);
  EXPECT_EQ(Utils::jsonToCanonicalStr(j), example_json);
}

TEST(xml2json, bad_input) {
  {
    // wrong xml
    std::stringstream inxml("<a");
    EXPECT_THROW(xml2json::xml2json(inxml), std::runtime_error);
  }
  {
    // too deep
    std::stringstream inxml("<a><a><a><a><a><a><a><a><a><a><a>xxx</a></a></a></a></a></a></a></a></a></a></a>");
    EXPECT_THROW(xml2json::xml2json(inxml), std::runtime_error);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
