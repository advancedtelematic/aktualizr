#include "utilities/utils.h"

#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include <archive.h>
#include <archive_entry.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <glob.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "aktualizr_version.h"
#include "logging/logging.h"

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
  int64_t paddingChars = std::count(base64_string.begin(), base64_string.end(), '=');
  std::replace(base64_string.begin(), base64_string.end(), '=', 'A');
  std::string result(base64_to_bin(base64_string.begin()), base64_to_bin(base64_string.end()));
  result.erase(result.end() - paddingChars, result.end());
  return result;
}

std::string Utils::toBase64(const std::string &tob64) {
  std::string b64sig(base64_text(tob64.begin()), base64_text(tob64.end()));
  b64sig.append((3 - tob64.length() % 3) % 3, '=');
  return b64sig;
}

// Strip leading and trailing quotes
std::string Utils::stripQuotes(const std::string &value) {
  std::string res = value;
  res.erase(std::remove(res.begin(), res.end(), '\"'), res.end());
  return res;
}

// Add leading and trailing quotes
std::string Utils::addQuotes(const std::string &value) { return "\"" + value + "\""; }

std::string Utils::extractField(const std::string &in, unsigned int field_id) {
  std::string out;
  auto it = in.begin();

  // skip spaces
  for (; it != in.end() && (isspace(*it) != 0); it++) {
    ;
  }
  for (unsigned int k = 0; k < field_id; k++) {
    bool empty = true;
    for (; it != in.end() && (isspace(*it) == 0); it++) {
      empty = false;
    }
    if (empty) {
      throw std::runtime_error(std::string("No such field ").append(std::to_string(field_id)));
    }
    for (; it != in.end() && (isspace(*it) != 0); it++) {
      ;
    }
  }

  for (; it != in.end() && (isspace(*it) == 0); it++) {
    out += *it;
  }
  return out;
}

Json::Value Utils::parseJSON(const std::string &json_str) {
  std::istringstream strs(json_str);
  Json::Value json_value;
  parseFromStream(Json::CharReaderBuilder(), strs, &json_value, nullptr);
  return json_value;
}

Json::Value Utils::parseJSONFile(const boost::filesystem::path &filename) {
  std::ifstream path_stream(filename.c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return Utils::parseJSON(content);
}

std::string Utils::genPrettyName() {
  std::random_device urandom;

  std::uniform_int_distribution<> adverbs_dist(0, (sizeof(adverbs) / sizeof(char *)) - 1);
  std::uniform_int_distribution<> nouns_dist(0, (sizeof(names) / sizeof(char *)) - 1);
  std::uniform_int_distribution<> digits(0, 999);
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

std::string Utils::readFile(const boost::filesystem::path &filename, const bool trim) {
  boost::filesystem::path tmpFilename = filename;
  tmpFilename += ".new";
  // that's kind of dangerous to include a specific use-case business logic
  // into a generic function used by many use cases
  // TODO: consider refactoring it, e.g. a generic readFile + a specific one with that includes
  // the ".new" file handling
  if (boost::filesystem::exists(tmpFilename)) {
    LOG_WARNING << tmpFilename << " was found on FS, removing";
    boost::filesystem::remove(tmpFilename);
  }
  std::ifstream path_stream(filename.c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());

  if (trim) {
    boost::trim_if(content, boost::is_any_of(" \t\r\n"));
  }
  return content;
}

static constexpr size_t BSIZE = 20 * 512;

struct archive_state {
  archive_state(std::istream &is_in) : is(is_in) {}
  std::istream &is;
  std::array<char, BSIZE> buf{};
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

void Utils::writeFile(const boost::filesystem::path &filename, const std::string &content, bool create_directories) {
  if (create_directories) {
    boost::filesystem::create_directories(filename.parent_path());
  }
  Utils::writeFile(filename, content.c_str(), content.size());
}

void Utils::writeFile(const boost::filesystem::path &filename, const char *content, size_t size) {
  // also replace the target file atomically by creating filename.new and
  // renaming it to the target file name
  boost::filesystem::path tmpFilename = filename;
  tmpFilename += ".new";

  std::ofstream file(tmpFilename.c_str());
  if (!file.good()) {
    throw std::runtime_error(std::string("Error opening file ") + tmpFilename.string());
  }
  file.write(content, static_cast<std::streamsize>(size));
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

std::string Utils::jsonToCanonicalStr(const Json::Value &json) {
  static Json::StreamWriterBuilder wbuilder = []() {
    Json::StreamWriterBuilder w;
    wbuilder["indentation"] = "";
    return w;
  }();
  return Json::writeString(wbuilder, json);
}

Json::Value Utils::getHardwareInfo() {
  std::string result;
  const int exit_code = shell("lshw -json", &result);

  if (exit_code != 0) {
    LOG_WARNING << "Could not execute lshw (is it installed?).";
    return Json::Value();
  }
  const Json::Value parsed = Utils::parseJSON(result);
  return (parsed.isArray()) ? parsed[0] : parsed;
}

Json::Value Utils::getNetworkInfo() {
  // get interface with default route
  std::ifstream path_stream("/proc/net/route");
  std::string route_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());

  struct Itf {
    std::string name = std::string();
    std::string ip = std::string();
    std::string mac = std::string();
  } itf;
  std::istringstream route_stream(route_content);
  std::array<char, 200> line{};

  // skip first line
  route_stream.getline(&line[0], line.size());
  while (route_stream.getline(&line[0], line.size())) {
    std::string itfn = Utils::extractField(&line[0], 0);
    std::string droute = Utils::extractField(&line[0], 1);
    if (droute == "00000000") {
      itf.name = itfn;
      // take the first routing to 0
      break;
    }
  }

  if (itf.name != "") {
    {
      // get ip address
      StructGuard<struct ifaddrs> ifaddrs(nullptr, freeifaddrs);
      {
        struct ifaddrs *ifa;
        if (getifaddrs(&ifa) < 0) {
          LOG_ERROR << "getifaddrs: " << std::strerror(errno);
        } else {
          ifaddrs.reset(ifa);
        }
      }
      if (ifaddrs != nullptr) {
        for (struct ifaddrs *ifa = ifaddrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
          if (itf.name == ifa->ifa_name) {
            if (ifa->ifa_addr == nullptr) {
              continue;
            }
            if (ifa->ifa_addr->sa_family != AF_INET) {
              continue;
            }
            const struct sockaddr_storage *sa = reinterpret_cast<struct sockaddr_storage *>(ifa->ifa_addr);

            itf.ip = Utils::ipDisplayName(*sa);
          }
        }
      }
    }
    {
      // get mac address
      std::ifstream mac_stream("/sys/class/net/" + itf.name + "/address");
      std::string m((std::istreambuf_iterator<char>(mac_stream)), std::istreambuf_iterator<char>());
      itf.mac = std::move(m);
      boost::trim_right(itf.mac);
    }
  }

  Json::Value network_info;
  network_info["local_ipv4"] = itf.ip;
  network_info["mac"] = itf.mac;
  network_info["hostname"] = Utils::getHostname();

  return network_info;
}

std::string Utils::getHostname() {
  char hostname[200];
  if (gethostname(hostname, 200) < 0) {
    return "";
  }
  return hostname;
}

std::string Utils::randomUuid() {
  std::random_device urandom;
  boost::uuids::basic_random_generator<std::random_device> uuid_gen(urandom);
  return boost::uuids::to_string(uuid_gen());
}

// FIXME Doesn't work with broken symlinks
void Utils::copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to) {
  boost::filesystem::remove_all(to);

  boost::filesystem::create_directories(to);
  boost::filesystem::directory_iterator it(from);

  for (; it != boost::filesystem::directory_iterator(); it++) {
    if (boost::filesystem::is_directory(it->path())) {
      copyDir(it->path(), to / it->path().filename());
    } else {
      boost::filesystem::copy_file(it->path(), to / it->path().filename());
    }
  }
}

std::string Utils::readFileFromArchive(std::istream &as, const std::string &filename, const bool trim) {
  struct archive *a = archive_read_new();
  if (a == nullptr) {
    LOG_ERROR << "archive error: could not initialize archive object";
    throw std::runtime_error("archive error");
  }
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  auto state = std_::make_unique<archive_state>(std::ref(as));
  int r = archive_read_open(a, reinterpret_cast<void *>(state.get()), nullptr, read_cb, nullptr);
  if (r != ARCHIVE_OK) {
    LOG_ERROR << "archive error: " << archive_error_string(a);
    archive_read_free(a);
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
      r = archive_read_data_block(a, reinterpret_cast<const void **>(&buff), &size, &offset);
      if (r == ARCHIVE_EOF) {
        found = true;
        break;
      }
      if (r != ARCHIVE_OK) {
        LOG_ERROR << "archive error: " << archive_error_string(a);
        break;
      }
      if (size > 0 && buff != nullptr) {
        out_stream.write(buff, static_cast<ssize_t>(size));
      }
    }
  }

  r = archive_read_free(a);
  if (r != ARCHIVE_OK) {
    LOG_ERROR << "archive error: " << archive_error_string(a);
  }

  if (!found) {
    throw std::runtime_error("could not extract " + filename + " from archive");
  }

  std::string result = out_stream.str();
  if (trim) {
    boost::trim_if(result, boost::is_any_of(" \t\r\n"));
  }
  return result;
}

static ssize_t write_cb(struct archive *a, void *client_data, const void *buffer, size_t length) {
  auto s = reinterpret_cast<std::ostream *>(client_data);
  s->write(reinterpret_cast<const char *>(buffer), static_cast<ssize_t>(length));
  if (s->fail()) {
    archive_set_error(a, -1, "unable to write in stream");
    return -1;
  }

  return static_cast<ssize_t>(length);
}

void Utils::writeArchive(const std::map<std::string, std::string> &entries, std::ostream &as) {
  struct archive *a = archive_write_new();
  if (a == nullptr) {
    LOG_ERROR << "archive error: could not initialize archive object";
    throw std::runtime_error("archive error");
  }
  archive_write_set_format_pax(a);
  archive_write_add_filter_gzip(a);

  int r = archive_write_open(a, reinterpret_cast<void *>(&as), nullptr, write_cb, nullptr);
  if (r != ARCHIVE_OK) {
    LOG_ERROR << "archive error: " << archive_error_string(a);
    archive_write_free(a);
    throw std::runtime_error("archive error");
  }

  struct archive_entry *entry = archive_entry_new();
  for (const auto &el : entries) {
    archive_entry_clear(entry);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, S_IRWXU | S_IRWXG | S_IRWXO);
    archive_entry_set_size(entry, static_cast<ssize_t>(el.second.size()));
    archive_entry_set_pathname(entry, el.first.c_str());
    if (archive_write_header(a, entry) != 0) {
      LOG_ERROR << "archive error: " << archive_error_string(a);
      archive_entry_free(entry);
      archive_write_free(a);
      throw std::runtime_error("archive error");
    }
    if (archive_write_data(a, el.second.c_str(), el.second.size()) < 0) {
      LOG_ERROR << "archive error: " << archive_error_string(a);
      archive_entry_free(entry);
      archive_write_free(a);
      throw std::runtime_error("archive error");
    }
  }
  archive_entry_free(entry);
  r = archive_write_free(a);
  if (r != ARCHIVE_OK) {
    LOG_ERROR << "archive error: " << archive_error_string(a);
  }
}

sockaddr_storage Utils::ipGetSockaddr(int fd) {
  sockaddr_storage ss{};
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
      const auto *sa = reinterpret_cast<const sockaddr_in *>(&saddr);
      inet_ntop(AF_INET, &sa->sin_addr, ipstr, sizeof(ipstr));
      return std::string(ipstr);
    }
    case AF_INET6: {
      const auto *sa = reinterpret_cast<const sockaddr_in6 *>(&saddr);
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
    const auto *sa = reinterpret_cast<const sockaddr_in *>(&saddr);
    p = sa->sin_port;
  } else if (saddr.ss_family == AF_INET6) {
    const auto *sa = reinterpret_cast<const sockaddr_in6 *>(&saddr);
    p = sa->sin6_port;
  } else {
    return -1;
  }

  return ntohs(p);
}

int Utils::shell(const std::string &command, std::string *output, bool include_stderr) {
  char buffer[128];
  std::string full_command(command);
  if (include_stderr) {
    full_command += " 2>&1";
  }
  FILE *pipe = popen(full_command.c_str(), "r");
  if (pipe == nullptr) {
    *output = "popen() failed!";
    return -1;
  }
  while (feof(pipe) == 0) {
    if (fgets(buffer, 128, pipe) != nullptr) {
      *output += buffer;
    }
  }
  int exitcode = pclose(pipe);
  return WEXITSTATUS(exitcode);
}

boost::filesystem::path Utils::absolutePath(const boost::filesystem::path &root, const boost::filesystem::path &file) {
  if (file.is_absolute() || root.empty()) {
    return file;
  }
  return (root / file);
}

// passing ext argument keep the leading point e.g. '.ext'
// results are returned sorted in alpanumeric order
std::vector<boost::filesystem::path> Utils::getDirEntriesByExt(const boost::filesystem::path &dir_path,
                                                               const std::string &ext) {
  std::vector<boost::filesystem::path> entries;
  boost::filesystem::directory_iterator entryItEnd, entryIt(dir_path);
  for (; entryIt != entryItEnd; ++entryIt) {
    auto &entry_path = entryIt->path();
    if (!boost::filesystem::is_directory(*entryIt) && entry_path.extension().string() == ext) {
      entries.push_back(entry_path);
    }
  }
  std::sort(entries.begin(), entries.end());
  return entries;
}

void Utils::createDirectories(const boost::filesystem::path &path, mode_t mode) {
  boost::filesystem::path parent = path.parent_path();
  if (!parent.empty() && !boost::filesystem::exists(parent)) {
    Utils::createDirectories(parent, mode);
  }
  if (mkdir(path.c_str(), mode) == -1) {
    throw std::runtime_error(std::string("could not create directory: ").append(path.native()));
  }
  std::cout << "created: " << path.native() << "\n";
}

bool Utils::createSecureDirectory(const boost::filesystem::path &path) {
  if (mkdir(path.c_str(), S_IRWXU) == 0) {
    // directory created successfully
    return true;
  }

  // mkdir failed, see if the directory already exists with correct permissions
  struct stat st {};
  int ret = stat(path.c_str(), &st);
  // checks: - stat succeeded
  //         - is a directory
  //         - no read and write permissions for group and others
  //         - owner is current user
  return (ret >= 0 && ((st.st_mode & S_IFDIR) == S_IFDIR) && (st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == S_IRWXU &&
          (st.st_uid == getuid()));
}

std::string Utils::urlEncode(const std::string &input) {
  std::string res;

  for (char c : input) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
        c == '_' || c == '~' || c == '/') {
      res.push_back(c);
    } else {
      res.push_back('%');
      auto nib = static_cast<char>(((c >> 4) & 0x0F));
      res.push_back(static_cast<char>((nib < 10) ? nib + '0' : nib - 10 + 'A'));
      nib = static_cast<char>(c & 0x0F);
      res.push_back(static_cast<char>((nib < 10) ? nib + '0' : nib - 10 + 'A'));
    }
  }
  return res;
}

CURL *Utils::curlDupHandleWrapper(CURL *const curl_in, const bool using_pkcs11) {
  CURL *curl = curl_easy_duphandle(curl_in);

  // This is a workaround for a bug in curl. It has been fixed in
  // 75a845d8cfa71688d59d43788c35829b25b6d6af (curl 7.61.1), but that is not
  // the default in most distributions yet, so we will continue to use the
  // workaround.
  if (using_pkcs11) {
    curlEasySetoptWrapper(curl, CURLOPT_SSLENGINE, "pkcs11");
  }
  return curl;
}

class SafeTempRoot {
 public:
  SafeTempRoot(const SafeTempRoot &) = delete;
  SafeTempRoot operator=(const SafeTempRoot &) = delete;
  // provide this as a static method so that we can use C++ static destructor
  // to remove the temp root
  static boost::filesystem::path &Get() {
    static SafeTempRoot r;

    return r.path;
  }

 private:
  SafeTempRoot() {
    boost::filesystem::path prefix = Utils::getStorageRootPath();
    if (prefix.empty()) {
      prefix = boost::filesystem::temp_directory_path();
    }
    boost::filesystem::path p = prefix / boost::filesystem::unique_path("aktualizr-%%%%-%%%%-%%%%-%%%%");
    if (mkdir(p.c_str(), S_IRWXU) == -1) {
      throw std::runtime_error(std::string("could not create temporary directory root: ").append(p.native()));
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

std::string Utils::storage_root_path_;

void Utils::setStorageRootPath(const std::string &storage_root_path) { storage_root_path_ = storage_root_path; }

boost::filesystem::path Utils::getStorageRootPath() { return storage_root_path_; }

void Utils::setUserAgent(std::string user_agent) { user_agent_ = std::move(user_agent); }

const char *Utils::getUserAgent() {
  if (user_agent_.empty()) {
    user_agent_ = (std::string("Aktualizr/") + aktualizr_version());
  }
  return user_agent_.c_str();
}

std::string Utils::user_agent_;

void Utils::setCaPath(boost::filesystem::path path) { ca_path_ = std::move(path); }

const char *Utils::getCaPath() { return ca_path_.c_str(); }

boost::filesystem::path Utils::ca_path_{"/etc/ssl/certs"};

TemporaryFile::TemporaryFile(const std::string &hint)
    : tmp_name_(SafeTempRoot::Get() / boost::filesystem::unique_path(std::string("%%%%-%%%%-").append(hint))) {}

TemporaryFile::~TemporaryFile() { boost::filesystem::remove(tmp_name_); }

void TemporaryFile::PutContents(const std::string &contents) {
  mode_t mode = S_IRUSR | S_IWUSR;
  int fd = open(Path().c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  if (fd < 0) {
    throw std::runtime_error(std::string("Could not write content to file: ") + Path().string() + ": " +
                             std::strerror(errno));
  }
  ssize_t written = write(fd, contents.c_str(), contents.size());
  close(fd);
  if (written < 0 || static_cast<size_t>(written) != contents.size()) {
    throw std::runtime_error(std::string("Could not write content to file: ") + Path().string());
  }
}

boost::filesystem::path TemporaryFile::Path() const { return tmp_name_; }

std::string TemporaryFile::PathString() const { return Path().string(); }

TemporaryDirectory::TemporaryDirectory(const std::string &hint)
    : tmp_name_(SafeTempRoot::Get() / boost::filesystem::unique_path(std::string("%%%%-%%%%-").append(hint))) {
  Utils::createDirectories(tmp_name_, S_IRWXU);
}

TemporaryDirectory::~TemporaryDirectory() { boost::filesystem::remove_all(tmp_name_); }

boost::filesystem::path TemporaryDirectory::Path() const { return tmp_name_; }

boost::filesystem::path TemporaryDirectory::operator/(const boost::filesystem::path &subdir) const {
  return (tmp_name_ / subdir);
}

std::string TemporaryDirectory::PathString() const { return Path().string(); }

void Utils::setSocketPort(sockaddr_storage *addr, in_port_t port) {
  if (addr->ss_family == AF_INET) {
    reinterpret_cast<sockaddr_in *>(addr)->sin_port = port;
  } else if (addr->ss_family == AF_INET6) {
    reinterpret_cast<sockaddr_in6 *>(addr)->sin6_port = port;
  }
}

bool operator<(const sockaddr_storage &left, const sockaddr_storage &right) {
  if (left.ss_family == AF_INET) {
    throw std::runtime_error("IPv4 addresses are not supported");
  }
  const unsigned char *left_addr = reinterpret_cast<const sockaddr_in6 *>(&left)->sin6_addr.s6_addr;    // NOLINT
  const unsigned char *right_addr = reinterpret_cast<const sockaddr_in6 *>(&right)->sin6_addr.s6_addr;  // NOLINT
  int res = memcmp(left_addr, right_addr, 16);

  return (res < 0);
}

Socket::Socket() {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == socket_fd_) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
}

Socket::~Socket() { ::close(socket_fd_); }

std::string Socket::toString() {
  auto saddr = Utils::ipGetSockaddr(socket_fd_);
  return Utils::ipDisplayName(saddr) + ":" + std::to_string(Utils::ipPort(saddr));
}

void Socket::bind(in_port_t port, bool reuse) {
  sockaddr_in sa{};
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  int reuseaddr = reuse ? 1 : 0;
  if (-1 == setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
    throw std::system_error(errno, std::system_category(), "socket");
  }

  if (-1 == ::bind(socket_fd_, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa))) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
}

ListenSocket::ListenSocket(in_port_t port) : _port(port) {
  bind(port);
  if (_port == 0) {
    // ephemeral port was bound, find out its real port number
    auto ephemeral_port = Utils::ipPort(Utils::ipGetSockaddr(socket_fd_));
    if (-1 != ephemeral_port) {
      _port = static_cast<in_port_t>(ephemeral_port);
    }
  }
}

ConnectionSocket::ConnectionSocket(const std::string &ip, in_port_t port, in_port_t bind_port)
    : remote_sock_address_{} {
  memset(&remote_sock_address_, 0, sizeof(remote_sock_address_));
  remote_sock_address_.sin_family = AF_INET;
  if (-1 == inet_pton(AF_INET, ip.c_str(), &(remote_sock_address_.sin_addr))) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
  remote_sock_address_.sin_port = htons(port);

  if (bind_port > 0) {
    bind(bind_port);
  }
}

ConnectionSocket::~ConnectionSocket() { ::shutdown(socket_fd_, SHUT_RDWR); }

int ConnectionSocket::connect() {
  return ::connect(socket_fd_, reinterpret_cast<const struct sockaddr *>(&remote_sock_address_),
                   sizeof(remote_sock_address_));
}

CurlEasyWrapper::CurlEasyWrapper() {
  handle = curl_easy_init();
  if (handle == nullptr) {
    throw std::runtime_error("Could not initialize curl handle");
  }
  curlEasySetoptWrapper(handle, CURLOPT_USERAGENT, Utils::getUserAgent());
}

CurlEasyWrapper::~CurlEasyWrapper() {
  if (handle != nullptr) {
    curl_easy_cleanup(handle);
  }
}
