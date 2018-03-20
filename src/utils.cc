#include "utils.h"

#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <archive.h>
#include <archive_entry.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/filesystem.hpp>
#include <boost/random/random_device.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "logging.h"

const char *adverbs[] = {
    "adorable", "acidic",     "ample",        "aromatic",   "artistic", "attractive", "basic",    "beautiful",
    "best",     "blissful",   "bubbly",       "celebrated", "cheap",    "chilly",     "cloudy",   "colorful",
    "colossal", "complete",   "conventional", "costly",     "creamy",   "crisp",      "dense",    "double",
    "dry",      "easy",       "every",        "exotic",     "expert",   "fake",       "fabulous", "fast",
    "fine",     "firm",       "first",        "flaky",      "flat",     "fluffy",     "frozen",   "generous",
    "giant",    "glass",      "glorious",     "golden",     "good",     "grand",      "great",    "half",
    "happy",    "hard",       "healthy",      "heavy",      "hot",      "huge",       "humble",   "ideal",
    "icy",      "incredible", "interesting",  "joyous",     "juicy",    "jumbo",      "large",    "late",
    "lavish",   "leafy",      "lean",         "light",      "lovely",   "marvelous",  "mature",   "modern",
    "modest",   "neat",       "new",          "nice",       "nifty",    "nutty",      "oily",     "ornate",
    "perfect",  "plain",      "posh",         "pretty",     "prime",    "proper",     "pure",     "quick",
    "raw",      "real",       "rich",         "ripe",       "safe",     "salty",      "several",  "short",
    "simple",   "slim",       "slow",         "smooth",     "soft",     "solid",      "speedy",   "spotless",
    "strong",   "stylish",    "subtle",       "super",      "sweet",    "tasty",      "tempting", "tender",
    "terrific", "thick",      "thin",         "tidy",       "tiny",     "twin",       "ultimate", "unique",
    "uniform",  "unusual",    "valuable",     "vast",       "warm",     "wavy",       "wet",      "whole",
    "wide",     "wild",       "wooden",       "young"};

const char *names[] = {"Allerlei",
                       "Apfelkuchen",
                       "Backerbsen",
                       "Baumkuchen",
                       "Beetenbartsch",
                       "Berliner",
                       "Bethmaennchen",
                       "Biersuppe",
                       "Birnenfladen",
                       "Bohnen",
                       "Bratapfel",
                       "Bratkartoffeln",
                       "Brezel",
                       "Broetchen",
                       "Butterkuchen",
                       "Currywurst",
                       "Dampfnudel",
                       "Dibbelabbes",
                       "Eierkuchen",
                       "Eintopf",
                       "Erbsensuppe",
                       "Flaedlesuppe",
                       "Flammkuchen",
                       "Fliederbeersuppe",
                       "Franzbroetchen",
                       "Funkenkuechlein",
                       "Gedadschde",
                       "Gemueseschnitzel",
                       "Germknoedel",
                       "Gerstensuppe",
                       "Griessbrei",
                       "Gruenkohl",
                       "Gruetze",
                       "Gummibaerchen",
                       "Gurkensalat",
                       "Habermus",
                       "Haddekuche",
                       "Hagebuttenmark",
                       "Handkaese",
                       "Herrencreme",
                       "Hoorische",
                       "Kaesekuchen",
                       "Kaiserschmarrn",
                       "Kartoffelpueree",
                       "Kartoffelpuffer",
                       "Kartoffelsalat",
                       "Kastanien",
                       "Kichererbsen",
                       "Kirschenmichel",
                       "Kirschtorte",
                       "Klaben",
                       "Kloesse",
                       "Kluntjes",
                       "Knaeckebrot",
                       "Kniekuechle",
                       "Knoedel",
                       "Kohlroulade",
                       "Krautfleckerl",
                       "Kuerbiskernbrot",
                       "Kuerbissuppe",
                       "Lebkuchen",
                       "Linsen",
                       "Loeffelerbsen",
                       "Magenbrot",
                       "Marillenknoedel",
                       "Maroni",
                       "Marsch",
                       "Marzipan",
                       "Maultaschen",
                       "Milliramstrudel",
                       "Mischbrot",
                       "Mohnnudeln",
                       "Mohnpielen",
                       "Mohnzelten",
                       "Muesli",
                       "Nussecke",
                       "Nusstorte",
                       "Palatschinke",
                       "Pellkartoffeln",
                       "Pfannkuchen",
                       "Pfefferkuchen",
                       "Pillekuchen",
                       "Pommes",
                       "Poschweck",
                       "Powidltascherl",
                       "Printen",
                       "Prinzregententorte",
                       "Pumpernickel",
                       "Punschkrapfen",
                       "Quarkkeulchen",
                       "Quetschkartoffeln",
                       "Raclette",
                       "Radi",
                       "Reibekuchen",
                       "Reinling",
                       "Riebel",
                       "Roeggelchen",
                       "Roesti",
                       "Sauerkraut",
                       "Schmalzkuchen",
                       "Schmorgurken",
                       "Schnippelbohnen",
                       "Schoeberl",
                       "Schrippe",
                       "Schupfnudel",
                       "Schuxen",
                       "Schwammerlsuppe",
                       "Schweineohren",
                       "Sonnenblumenkernbrot",
                       "Spaetzle",
                       "Spaghettieis",
                       "Spargel",
                       "Spekulatius",
                       "Springerle",
                       "Spritzkuchen",
                       "Stampfkartoffeln",
                       "Sterz",
                       "Stollen",
                       "Streuselkuchen",
                       "Tilsit",
                       "Toastbrot",
                       "Topfenstrudel",
                       "Vollkornbrot",
                       "Wibele",
                       "Wickelkloesse",
                       "Zimtwaffeln",
                       "Zwetschkenroester",
                       "Zwiebelkuchen"};

typedef boost::archive::iterators::base64_from_binary<
    boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8> >
    base64_text;

typedef boost::archive::iterators::transform_width<
    boost::archive::iterators::binary_from_base64<
        boost::archive::iterators::remove_whitespace<std::string::const_iterator> >,
    8, 6>
    base64_to_bin;

std::string Utils::fromBase64(std::string base64_string) {
  unsigned long long paddingChars = std::count(base64_string.begin(), base64_string.end(), '=');
  std::replace(base64_string.begin(), base64_string.end(), '=', 'A');
  std::string result(base64_to_bin(base64_string.begin()), base64_to_bin(base64_string.end()));
  result.erase(result.end() - static_cast<unsigned int>(paddingChars), result.end());
  return result;
}

std::string Utils::toBase64(const std::string &tob64) {
  std::string b64sig(base64_text(tob64.begin()), base64_text(tob64.end()));
  b64sig.append((3 - tob64.length() % 3) % 3, '=');
  return b64sig;
}

void Utils::hex2bin(const std::string hexstring, unsigned char *binout) {
  for (int i = 0; i < hexstring.length(); i += 2) {
    char hex_byte[3];
    hex_byte[0] = hexstring[i];
    hex_byte[1] = hexstring[i + 1];
    hex_byte[2] = 0;
    binout[i / 2] = strtol(hex_byte, NULL, 16);
  }
}

// Strip leading and trailing quotes
std::string Utils::stripQuotes(const std::string &value) {
  std::string res = value;
  res.erase(std::remove(res.begin(), res.end(), '\"'), res.end());
  return res;
}

// Add leading and trailing quotes
std::string Utils::addQuotes(const std::string &value) { return "\"" + value + "\""; }

Json::Value Utils::parseJSON(const std::string &json_str) {
  Json::Reader reader;
  Json::Value json_value;
  reader.parse(json_str, json_value);
  return json_value;
}

Json::Value Utils::parseJSONFile(const boost::filesystem::path &filename) {
  std::ifstream path_stream(filename.c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return Utils::parseJSON(content);
}

std::string Utils::genPrettyName() {
  boost::random::random_device urandom;

  boost::random::uniform_int_distribution<> adverbs_dist(0, (sizeof(adverbs) / sizeof(char *)) - 1);
  boost::random::uniform_int_distribution<> nouns_dist(0, (sizeof(names) / sizeof(char *)) - 1);
  boost::random::uniform_int_distribution<> digits(0, 999);
  std::stringstream pretty_name;
  pretty_name << adverbs[adverbs_dist(urandom)];
  pretty_name << "-";
  pretty_name << names[nouns_dist(urandom)];
  pretty_name << "-";
  pretty_name << digits(urandom);
  std::string res = pretty_name.str();
  std::transform(res.begin(), res.end(), res.begin(), ::tolower);
  return res;
}

std::string Utils::readFile(const boost::filesystem::path &filename) {
  boost::filesystem::path tmpFilename = filename;
  tmpFilename += ".new";

  if (boost::filesystem::exists(tmpFilename)) {
    LOG_WARNING << tmpFilename << " was found on FS, removing";
    boost::filesystem::remove(tmpFilename);
  }
  std::ifstream path_stream(filename.c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return content;
}

static constexpr size_t BSIZE = 20 * 512;

struct archive_state {
  std::istream &is;
  std::array<char, BSIZE> buf;
};

static ssize_t read_cb(struct archive *a, void *client_data, const void **buffer) {
  auto s = reinterpret_cast<archive_state *>(client_data);
  if (s->is.fail()) {
    archive_set_error(a, -1, "unable to read from stream");
    return 0;
  }
  if (s->is.eof()) {
    return 0;
  }
  s->is.read(s->buf.data(), BSIZE);
  if (!s->is.eof() && s->is.fail()) {
    archive_set_error(a, -1, "unable to read from stream");
    return 0;
  }
  *buffer = s->buf.data();

  return s->is.gcount();
}

std::string Utils::readFileFromArchive(std::istream &as, const std::string &filename) {
  struct archive *a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  archive_state state = {as, {}};
  int r = archive_read_open(a, reinterpret_cast<void *>(&state), nullptr, read_cb, nullptr);
  if (r != ARCHIVE_OK) {
    LOG_ERROR << "archive error: " << archive_error_string(a);
    throw std::runtime_error("archive error");
  }

  bool found = false;
  std::stringstream out_stream;
  struct archive_entry *entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    if (filename != archive_entry_pathname(entry)) {
      archive_read_data_skip(a);
      continue;
    }

    const char *buff;
    size_t size;
    int64_t offset;

    for (;;) {
      r = archive_read_data_block(a, (const void **)&buff, &size, &offset);
      if (r == ARCHIVE_EOF) {
        found = true;
        break;
      } else if (r != ARCHIVE_OK) {
        LOG_ERROR << "archive error: " << archive_error_string(a);
        break;
      } else if (size > 0 && buff != NULL) {
        out_stream.write(buff, size);
      }
    }
  }

  r = archive_read_free(a);
  if (r != ARCHIVE_OK) LOG_ERROR << "archive error: " << archive_error_string(a);

  if (!found) {
    throw std::runtime_error("could not extract " + filename + " from archive");
  }

  return out_stream.str();
}

void Utils::writeFile(const boost::filesystem::path &filename, const std::string &content, bool create_directories) {
  // also replace the target file atomically by creating filename.new and
  // renaming it to the target file name
  boost::filesystem::path tmpFilename = filename;
  tmpFilename += ".new";

  if (create_directories) {
    boost::filesystem::create_directories(filename.parent_path());
  }
  std::ofstream file(tmpFilename.c_str());
  if (!file.good()) {
    throw std::runtime_error(std::string("Error opening file ") + tmpFilename.string());
  }
  file << content;
  file.close();

  boost::filesystem::rename(tmpFilename, filename);
}

void Utils::writeFile(const boost::filesystem::path &filename, const Json::Value &content, bool create_directories) {
  Utils::writeFile(filename, jsonToStr(content), create_directories);
}

std::string Utils::jsonToStr(const Json::Value &json) {
  std::stringstream ss;
  ss << json;
  return ss.str();
}

Json::Value Utils::getHardwareInfo() {
  std::string result = "";
  int exit_code = shell("lshw -json", &result);

  if (exit_code) {
    LOG_WARNING << "Could not execute lshw (is it installed?).";
    return Json::Value();
  } else {
    return Utils::parseJSON(result);
  }
}

std::string Utils::getHostname() {
  char hostname[200];
  if (gethostname(hostname, 200) < 0) {
    return "";
  }
  return hostname;
}

std::string Utils::randomUuid() {
  boost::random::random_device urandom;
  boost::uuids::basic_random_generator<boost::random::random_device> uuid_gen(urandom);
  return boost::uuids::to_string(uuid_gen());
}

void Utils::copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to) {
  boost::filesystem::remove_all(to);

  boost::filesystem::create_directories(to);
  boost::filesystem::directory_iterator it(from);

  for (; it != boost::filesystem::directory_iterator(); it++) {
    if (boost::filesystem::is_directory(it->path()))
      copyDir(it->path(), to / it->path().filename());
    else
      boost::filesystem::copy_file(it->path(), to / it->path().filename());
  }
}

sockaddr_storage Utils::ipGetSockaddr(int fd) {
  sockaddr_storage ss;
  socklen_t len = sizeof(ss);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&ss), &len) < 0) {
    throw std::runtime_error(std::string("Could not get sockaddr: ") + std::strerror(errno));
  }

  return ss;
}

std::string Utils::ipDisplayName(const sockaddr_storage &saddr) {
  char ipstr[INET6_ADDRSTRLEN];

  switch (saddr.ss_family) {
    case AF_INET: {
      const sockaddr_in *sa = reinterpret_cast<const sockaddr_in *>(&saddr);
      inet_ntop(AF_INET, &sa->sin_addr, ipstr, sizeof(ipstr));
      return std::string(ipstr);
    }
    case AF_INET6: {
      const sockaddr_in6 *sa = reinterpret_cast<const sockaddr_in6 *>(&saddr);
      inet_ntop(AF_INET6, &sa->sin6_addr, ipstr, sizeof(ipstr));
      return std::string(ipstr);
    }
    default:
      return "unknown";
  }
}

int Utils::ipPort(const sockaddr_storage &saddr) {
  in_port_t p;
  if (saddr.ss_family == AF_INET) {
    const sockaddr_in *sa = reinterpret_cast<const sockaddr_in *>(&saddr);
    p = sa->sin_port;
  } else if (saddr.ss_family == AF_INET6) {
    const sockaddr_in6 *sa = reinterpret_cast<const sockaddr_in6 *>(&saddr);
    p = sa->sin6_port;
  } else {
    return -1;
  }

  return ntohs(p);
}

int Utils::shell(const std::string &command, std::string *output, bool include_stderr) {
  char buffer[128];
  std::string full_command(command);
  if (include_stderr) full_command += " 2>&1";
  FILE *pipe = popen(full_command.c_str(), "r");
  if (!pipe) {
    *output = "popen() failed!";
    return -1;
  }
  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != NULL) {
      *output += buffer;
    }
  }
  int exitcode = pclose(pipe);
  return WEXITSTATUS(exitcode);
}

boost::filesystem::path Utils::absolutePath(const boost::filesystem::path &root, const boost::filesystem::path &file) {
  if (file.is_absolute() || root.empty()) {
    return file;
  } else {
    return (root / file);
  }
}

class SafeTempRoot : private boost::noncopyable {
 public:
  // provide this as a static method so that we can use C++ static destructor
  // to remove the temp root
  static boost::filesystem::path &Get() {
    static SafeTempRoot r;

    return r.path;
  }

 private:
  SafeTempRoot() {
    boost::filesystem::path p =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("aktualizr-%%%%-%%%%-%%%%-%%%%");

    if (mkdir(p.c_str(), S_IRWXU) == -1) {
      throw std::runtime_error("could not create temporary directory root: " + p.native());
    }

    path = boost::filesystem::path(p);
  }
  ~SafeTempRoot() {
    try {
      boost::filesystem::remove_all(path);
    } catch (...) {
      // ignore this, not critical
    }
  }

  boost::filesystem::path path;
};

TemporaryFile::TemporaryFile(const std::string &hint)
    : tmp_name_(SafeTempRoot::Get() / boost::filesystem::unique_path("%%%%-%%%%-" + hint)) {}

TemporaryFile::~TemporaryFile() { boost::filesystem::remove(tmp_name_); }

void TemporaryFile::PutContents(const std::string &contents) {
  std::ofstream out(Path().c_str());
  out << contents;
}

boost::filesystem::path TemporaryFile::Path() const { return tmp_name_; }

std::string TemporaryFile::PathString() const { return Path().string(); }

TemporaryDirectory::TemporaryDirectory(const std::string &hint)
    : tmp_name_(SafeTempRoot::Get() / boost::filesystem::unique_path("%%%%-%%%%-" + hint)) {
  boost::filesystem::create_directories(tmp_name_);
}

TemporaryDirectory::~TemporaryDirectory() { boost::filesystem::remove_all(tmp_name_); }

boost::filesystem::path TemporaryDirectory::Path() const { return tmp_name_; }

boost::filesystem::path TemporaryDirectory::operator/(const boost::filesystem::path &subdir) const {
  return (tmp_name_ / subdir);
}

std::string TemporaryDirectory::PathString() const { return Path().string(); }
