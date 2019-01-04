#ifndef UTILS_H_
#define UTILS_H_

#include <glob.h>
#include <boost/filesystem.hpp>
#include <memory>
#include <string>

#include <curl/curl.h>
#include <netinet/in.h>

#include "json/json.h"

struct Utils {
  static std::string fromBase64(std::string base64_string);
  static std::string toBase64(const std::string &tob64);
  static std::string stripQuotes(const std::string &value);
  static std::string addQuotes(const std::string &value);
  static std::string extractField(const std::string &in, unsigned int field_id);
  static Json::Value parseJSON(const std::string &json_str);
  static Json::Value parseJSONFile(const boost::filesystem::path &filename);
  static std::string jsonToStr(const Json::Value &json);
  static std::string jsonToCanonicalStr(const Json::Value &json);
  static std::string genPrettyName();
  static std::string readFile(const boost::filesystem::path &filename, bool trim = false);
  static void writeFile(const boost::filesystem::path &filename, const std::string &content,
                        bool create_directories = true, bool atomic = false);
  static void writeFile(const boost::filesystem::path &filename, const Json::Value &content,
                        bool create_directories = true, bool atomic = false);
  static void copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to);
  static std::string readFileFromArchive(std::istream &as, const std::string &filename, bool trim = false);
  static void writeArchive(const std::map<std::string, std::string> &entries, std::ostream &as);
  static Json::Value getHardwareInfo();
  static Json::Value getNetworkInfo();
  static std::string getHostname();
  static std::string randomUuid();
  static sockaddr_storage ipGetSockaddr(int fd);
  static std::string ipDisplayName(const sockaddr_storage &saddr);
  static int ipPort(const sockaddr_storage &saddr);
  static void clearUbootCounter();
  static void setUbootUpgraded();
  static int shell(const std::string &command, std::string *output, bool include_stderr = false);
  static boost::filesystem::path absolutePath(const boost::filesystem::path &root, const boost::filesystem::path &file);
  static void setSocketPort(sockaddr_storage *addr, in_port_t port);
  static std::vector<boost::filesystem::path> glob(const std::string &pat);
  static void createDirectories(const boost::filesystem::path &path, mode_t mode);
  static std::string urlEncode(const std::string &input);
  static CURL *curlDupHandleWrapper(CURL *curl_in, bool using_pkcs11);
};

/**
 * RAII Temporary file creation
 */
class TemporaryFile {
 public:
  explicit TemporaryFile(const std::string &hint = "file");
  TemporaryFile(const TemporaryFile &) = delete;
  TemporaryFile operator=(const TemporaryFile &) = delete;
  ~TemporaryFile();
  void PutContents(const std::string &contents);
  boost::filesystem::path Path() const;
  std::string PathString() const;

 private:
  boost::filesystem::path tmp_name_;
};

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(const std::string &hint = "dir");
  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory operator=(TemporaryDirectory &) = delete;
  ~TemporaryDirectory();
  boost::filesystem::path Path() const;
  std::string PathString() const;
  boost::filesystem::path operator/(const boost::filesystem::path &subdir) const;

 private:
  boost::filesystem::path tmp_name_;
};

// Can represent an absolute or relative path, only readable through the
// `.get()` method
//
// The intent is to avoid unintentional use of the "naked" relative path by
// mandating a base directory for each instantiation
class BasedPath {
 public:
  BasedPath(boost::filesystem::path p) : p_(std::move(p)) {}

  boost::filesystem::path get(const boost::filesystem::path &base) const {
    // note: BasedPath(bp.get()) == bp
    return Utils::absolutePath(base, p_);
  }

  bool empty() const { return p_.empty(); }
  bool operator==(const BasedPath &b) const { return p_ == b.p_; }
  bool operator!=(const BasedPath &b) const { return !(*this == b); }

 private:
  boost::filesystem::path p_;
};

// helper template for C (mostly openssl) data structured
//   user should still take care about the order of destruction
//   by instantiating StructGuard<> in a right order.
//   BTW local variables are destructed in reverse order of instantiation
template <typename T>
using StructGuard = std::unique_ptr<T, void (*)(T *)>;

// helper object for RAII socket management
struct SocketCloser {
  void operator()(const int *ptr) const {
    close(*ptr);
    delete ptr;
  }
};

using SocketHandle = std::unique_ptr<int, SocketCloser>;
bool operator<(const sockaddr_storage &left, const sockaddr_storage &right);  // required by std::map

// wrapper for curl handles
class CurlEasyWrapper {
 public:
  CurlEasyWrapper() {
    handle = curl_easy_init();
    if (handle == nullptr) {
      throw std::runtime_error("Could not initialize curl handle");
    }
  }
  ~CurlEasyWrapper() {
    if (handle != nullptr) {
      curl_easy_cleanup(handle);
    }
  }
  CURL *get() { return handle; }

 private:
  CURL *handle;
};

template <typename... T>
static void curlEasySetoptWrapper(CURL *curl_handle, CURLoption option, T &&... args) {
  const CURLcode retval = curl_easy_setopt(curl_handle, option, std::forward<T>(args)...);
  if (retval != 0u) {
    throw std::runtime_error(std::string("curl_easy_setopt error: ") + curl_easy_strerror(retval));
  }
}

// this is reference implementation of make_unique which is not yet included to C++11
namespace std_ {
template <class T>
struct _Unique_if {
  using _Single_object = std::unique_ptr<T>;
};

template <class T>
struct _Unique_if<T[]> {
  using _Unknown_bound = std::unique_ptr<T[]>;
};

template <class T, size_t N>
struct _Unique_if<T[N]> {
  using _Known_bound = void;
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args &&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
  using U = typename std::remove_extent<T>::type;
  return std::unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args &&...) = delete;
}  // namespace std_

#endif  // UTILS_H_
