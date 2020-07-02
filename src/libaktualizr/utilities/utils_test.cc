/**
 * \file
 */

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <fstream>
#include <map>
#include <random>
#include <set>

#include <boost/algorithm/hex.hpp>
#include <boost/archive/iterators/dataflow_exception.hpp>

#include "utilities/utils.h"

bool CharOk(char c) {
  if (('a' <= c) && (c <= 'z')) {
    return true;
  } else if (('0' <= c) && (c <= '9')) {
    return true;
  } else if (c == '-') {
    return true;
  } else {
    return false;
  }
}

bool PrettyNameOk(const std::string &name) {
  if (name.size() < 5) {
    return false;
  }
  for (std::string::const_iterator c = name.begin(); c != name.end(); ++c) {
    if (!CharOk(*c)) {
      return false;
    }
  }
  return true;
}

TEST(Utils, PrettyNameOk) {
  EXPECT_TRUE(PrettyNameOk("foo-bar-123"));
  EXPECT_FALSE(PrettyNameOk("NoCapitals"));
  EXPECT_FALSE(PrettyNameOk(""));
  EXPECT_FALSE(PrettyNameOk("foo-bar-123&"));
}

TEST(Utils, parseJSON) {
  // this should not cause valgrind warnings
  Utils::parseJSON("");
}

TEST(Utils, jsonToCanonicalStr) {
  const std::string sample = " { \"b\": 0, \"a\": [1, 2, {}], \"0\": \"x\"}";
  Json::Value parsed;

  parsed = Utils::parseJSON(sample);
  EXPECT_EQ(Utils::jsonToCanonicalStr(parsed), "{\"0\":\"x\",\"a\":[1,2,{}],\"b\":0}");

  const std::string sample2 = "0";
  parsed = Utils::parseJSON(sample2);
  EXPECT_EQ(Utils::jsonToCanonicalStr(parsed), "0");
}

/* Read hardware info from the system. */
TEST(Utils, getHardwareInfo) {
  Json::Value hwinfo = Utils::getHardwareInfo();
  EXPECT_NE(hwinfo, Json::Value());
  EXPECT_FALSE(hwinfo.isArray());
}

/* Read networking info from the system. */
TEST(Utils, getNetworkInfo) {
  Json::Value netinfo = Utils::getNetworkInfo();
  EXPECT_NE(netinfo["local_ipv4"].asString(), "");
  EXPECT_NE(netinfo["mac"].asString(), "");
  EXPECT_NE(netinfo["hostname"].asString(), "");
}

/* Read the hostname from the system. */
TEST(Utils, getHostname) { EXPECT_NE(Utils::getHostname(), ""); }

/**
 * Check that aktualizr can generate a pet name.
 *
 * Try 100 times and check that no duplicate names are produced. Tolerate 1
 * duplicate, since we have actually seen it before and it is statistically
 * not unreasonable with our current inputs.
 */
TEST(Utils, GenPrettyNameSane) {
  RecordProperty("zephyr_key", "OTA-986,TST-144");
  std::set<std::string> names;
  for (int i = 0; i < 100; i++) {
    const std::string name = Utils::genPrettyName();
    names.insert(name);
    const auto count = names.count(name);
    if (count > 2) {
      FAIL() << "Something wrong with randomness: " << name;
    } else if (count == 2) {
      std::cerr << "Lucky draw: " << name;
    }
  }
}

/* Generate a random UUID. */
TEST(Utils, RandomUuidSane) {
  std::set<std::string> uuids;
  for (int i = 0; i < 1000; i++) {
    std::string uuid = Utils::randomUuid();
    EXPECT_EQ(0, uuids.count(uuid));
    uuids.insert(uuid);
    EXPECT_EQ(1, uuids.count(uuid));
  }
}

TEST(Utils, ToBase64) {
  // Generated using python's base64.b64encode
  EXPECT_EQ("aGVsbG8=", Utils::toBase64("hello"));
  EXPECT_EQ("", Utils::toBase64(""));
  EXPECT_EQ("CQ==", Utils::toBase64("\t"));
  EXPECT_EQ("YWI=", Utils::toBase64("ab"));
  EXPECT_EQ("YWJj", Utils::toBase64("abc"));
}

TEST(Utils, FromBase64) {
  EXPECT_EQ(Utils::fromBase64("aGVsbG8="), "hello");
  EXPECT_EQ(Utils::fromBase64(""), "");
  EXPECT_EQ(Utils::fromBase64("YWI="), "ab");
  EXPECT_EQ(Utils::fromBase64("YWJj"), "abc");
}

TEST(Utils, FromBase64Wrong) {
  EXPECT_THROW(Utils::fromBase64("Привіт"), boost::archive::iterators::dataflow_exception);
  EXPECT_THROW(Utils::fromBase64("aGVsbG8=="), boost::archive::iterators::dataflow_exception);
  EXPECT_THROW(Utils::fromBase64("CQ==="), boost::archive::iterators::dataflow_exception);
}

TEST(Utils, Base64RoundTrip) {
  std::mt19937 gen;
  std::uniform_int_distribution<char> chars(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

  std::uniform_int_distribution<int> length(0, 20);

  for (int test = 0; test < 100; test++) {
    int len = length(gen);
    std::string original;
    for (int i = 0; i < len; i++) {
      original += chars(gen);
    }
    std::string b64 = Utils::toBase64(original);
    std::string output = Utils::fromBase64(b64);
    EXPECT_EQ(original, output);
  }
}

/* Extract credentials from a provided archive. */
TEST(Utils, ArchiveRead) {
  const std::string archive_path = "tests/test_data/credentials.zip";

  {
    std::ifstream as(archive_path, std::ios::binary | std::ios::in);
    EXPECT_FALSE(as.fail());
    EXPECT_THROW(Utils::readFileFromArchive(as, "bogus_filename"), std::runtime_error);
  }

  {
    std::ifstream as(archive_path, std::ios::binary | std::ios::in);
    EXPECT_FALSE(as.fail());
    std::string url = Utils::readFileFromArchive(as, "autoprov.url");
    EXPECT_EQ(url.rfind("https://", 0), 0);
  }
}

TEST(Utils, ArchiveWrite) {
  std::string archive_bytes;
  {
    std::map<std::string, std::string> fm{{"test", "A"}};
    std::stringstream as;
    Utils::writeArchive(fm, as);
    archive_bytes = as.str();
  }

  {
    std::stringstream as(archive_bytes);
    EXPECT_EQ(Utils::readFileFromArchive(as, "test"), "A");
  }
}

/* Remove credentials from a provided archive. */
TEST(Utils, ArchiveRemoveFile) {
  const boost::filesystem::path old_path = "tests/test_data/credentials.zip";
  TemporaryDirectory temp_dir;
  const boost::filesystem::path new_path = temp_dir.Path() / "credentials.zip";
  boost::filesystem::copy_file(old_path, new_path);

  Utils::removeFileFromArchive(new_path, "autoprov_credentials.p12");
  EXPECT_THROW(Utils::removeFileFromArchive(new_path, "bogus_filename"), std::runtime_error);

  {
    std::ifstream as(new_path.c_str(), std::ios::binary | std::ios::in);
    EXPECT_FALSE(as.fail());
    EXPECT_THROW(Utils::readFileFromArchive(as, "autoprov_credentials.p12"), std::runtime_error);
  }

  // Make sure the URL is still there. That's the only file that should be left
  // under normal circumstances.
  {
    std::ifstream as_old(old_path.c_str(), std::ios::binary | std::ios::in);
    EXPECT_FALSE(as_old.fail());
    std::ifstream as_new(new_path.c_str(), std::ios::binary | std::ios::in);
    EXPECT_FALSE(as_new.fail());
    EXPECT_EQ(Utils::readFileFromArchive(as_old, "autoprov.url"), Utils::readFileFromArchive(as_new, "autoprov.url"));
  }
}

/* Create a temporary directory. */
TEST(Utils, TemporaryDirectory) {
  boost::filesystem::path p;
  {
    TemporaryDirectory f("ahint");
    p = f.Path();
    EXPECT_TRUE(boost::filesystem::exists(p));               // The dir should exist
    EXPECT_NE(p.string().find("ahint"), std::string::npos);  // The hint is included in the filename

    struct stat statbuf {};
    EXPECT_GE(stat(p.parent_path().c_str(), &statbuf), 0);
    EXPECT_EQ(statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), S_IRWXU);
  }
  EXPECT_FALSE(boost::filesystem::exists(p));
}

/* Create a temporary file. */
TEST(Utils, TemporaryFile) {
  boost::filesystem::path p;
  {
    TemporaryFile f("ahint");
    p = f.Path();
    EXPECT_FALSE(boost::filesystem::exists(p));  // The file shouldn't already exist
    std::ofstream file(p.c_str());
    file << "test";
    file.close();
    EXPECT_TRUE(file);                                       // The write succeeded
    EXPECT_TRUE(boost::filesystem::exists(p));               // The file should exist here
    EXPECT_NE(p.string().find("ahint"), std::string::npos);  // The hint is included in the filename

    struct stat statbuf {};
    EXPECT_GE(stat(p.parent_path().c_str(), &statbuf), 0);
    EXPECT_EQ(statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), S_IRWXU);
  }
  EXPECT_FALSE(boost::filesystem::exists(p));  // The file gets deleted by the RAII dtor
}

/* Write to a temporary file. */
TEST(Utils, TemporaryFilePutContents) {
  TemporaryFile f("ahint");
  f.PutContents("thecontents");
  EXPECT_TRUE(boost::filesystem::exists(f.Path()));
  std::ifstream a(f.Path().c_str());
  std::string b;
  a >> b;
  EXPECT_EQ(b, "thecontents");
}

TEST(Utils, copyDir) {
  TemporaryDirectory temp_dir;

  Utils::writeFile(temp_dir.Path() / "from/1/foo", std::string("foo"));
  Utils::writeFile(temp_dir.Path() / "from/1/2/bar", std::string("bar"));
  Utils::writeFile(temp_dir.Path() / "from/1/2/baz", std::string("baz"));

  Utils::copyDir(temp_dir.Path() / "from", temp_dir.Path() / "to");
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to/1"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to/1/foo"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to/1/2"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to/1/2/bar"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "to/1/2/baz"));
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "to/1/foo"), "foo");
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "to/1/2/bar"), "bar");
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "to/1/2/baz"), "baz");
}

TEST(Utils, writeFileWithoutDirAutoCreation) {
  TemporaryDirectory temp_dir;

  boost::filesystem::create_directories(temp_dir.Path() / "1/2");
  Utils::writeFile(temp_dir.Path() / "1/foo", std::string("foo"), false);
  Utils::writeFile(temp_dir.Path() / "1/2/bar", std::string("bar"), false);

  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/foo"), "foo");
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/2/bar"), "bar");
}

TEST(Utils, writeFileWithDirAutoCreation) {
  TemporaryDirectory temp_dir;

  Utils::writeFile(temp_dir.Path() / "1/foo", std::string("foo"), true);
  Utils::writeFile(temp_dir.Path() / "1/2/bar", std::string("bar"), true);

  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/foo"), "foo");
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/2/bar"), "bar");
}

TEST(Utils, writeFileWithDirAutoCreationDefault) {
  TemporaryDirectory temp_dir;

  Utils::writeFile(temp_dir.Path() / "1/foo", std::string("foo"));
  Utils::writeFile(temp_dir.Path() / "1/2/bar", std::string("bar"));

  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/foo"), "foo");
  EXPECT_EQ(Utils::readFile(temp_dir.Path() / "1/2/bar"), "bar");
}

TEST(Utils, writeFileWithoutDirAutoCreationException) {
  TemporaryDirectory temp_dir;

  try {
    Utils::writeFile(temp_dir.Path() / "1/foo", std::string("foo"), false);
    EXPECT_TRUE(false);
  } catch (...) {
  }
}

TEST(Utils, writeFileJson) {
  TemporaryDirectory temp_dir;

  Json::Value val;
  val["key"] = "val";

  Utils::writeFile(temp_dir.Path() / "1/foo", val);
  Json::Value result_json = Utils::parseJSONFile(temp_dir.Path() / "1/foo");
  EXPECT_EQ(result_json["key"].asString(), val["key"].asString());
}

TEST(Utils, shell) {
  std::string out;
  int statuscode = Utils::shell("ls /", &out);
  EXPECT_EQ(statuscode, 0);

  statuscode = Utils::shell("ls /nonexistentdir123", &out);
  EXPECT_NE(statuscode, 0);
}

TEST(Utils, createSecureDirectory) {
  TemporaryDirectory temp_dir;

  EXPECT_TRUE(Utils::createSecureDirectory(temp_dir / "sec"));
  EXPECT_TRUE(boost::filesystem::is_directory(temp_dir / "sec"));
  // check that it succeeds the second time
  EXPECT_TRUE(Utils::createSecureDirectory(temp_dir / "sec"));

  // regular directory is insecure
  boost::filesystem::create_directory(temp_dir / "unsec");
  EXPECT_FALSE(Utils::createSecureDirectory(temp_dir / "unsec"));
}

TEST(Utils, urlencode) { EXPECT_EQ(Utils::urlEncode("test! test@ test#"), "test%21%20test%40%20test%23"); }

TEST(Utils, getDirEntriesByExt) {
  TemporaryDirectory temp_dir;

  Utils::writeFile(temp_dir / "02-test.toml", std::string(""));
  Utils::writeFile(temp_dir / "0x-test.noml", std::string(""));
  Utils::writeFile(temp_dir / "03-test.toml", std::string(""));
  Utils::writeFile(temp_dir / "01-test.toml", std::string(""));

  auto files = Utils::getDirEntriesByExt(temp_dir.Path(), ".toml");
  std::vector<boost::filesystem::path> expected{temp_dir / "01-test.toml", temp_dir / "02-test.toml",
                                                temp_dir / "03-test.toml"};
  EXPECT_EQ(files, expected);
}

TEST(Utils, BasedPath) {
  BasedPath bp("a/test.xml");

  EXPECT_EQ(BasedPath(bp.get("")), bp);
  EXPECT_EQ(bp.get("/"), "/a/test.xml");
  EXPECT_EQ(bp.get("/x"), "/x/a/test.xml");

  BasedPath abp("/a/test.xml");

  EXPECT_EQ(abp.get(""), "/a/test.xml");
  EXPECT_EQ(abp.get("/root/var"), "/a/test.xml");
}

TEST(Utils, TrimNewline) {
  TemporaryFile f("newline");
  std::string input = "test with newline";
  Utils::writeFile(f.Path(), input + "\n");
  std::string output = Utils::readFile(f.Path(), false);
  EXPECT_EQ(output, input + "\n");
  output = Utils::readFile(f.Path(), true);
  EXPECT_EQ(output, input);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
