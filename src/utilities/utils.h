#ifndef UTILS_H_
#define UTILS_H_

#include <glob.h>
#include <boost/filesystem.hpp>
#include <memory>
#include <string>

#include <netinet/in.h>

#include "json/json.h"

struct Utils {
  static std::string fromBase64(std::string);
  static std::string toBase64(const std::string &);
  static std::string stripQuotes(const std::string &value);
  static std::string addQuotes(const std::string &value);
  static std::string extractField(const std::string &in, unsigned int field_id);
  static Json::Value parseJSON(const std::string &json_str);
  static Json::Value parseJSONFile(const boost::filesystem::path &filename);
  static std::string jsonToStr(const Json::Value &json);
  static std::string genPrettyName();
  static std::string readFile(const boost::filesystem::path &filename);
  static void writeFile(const boost::filesystem::path &filename, const std::string &content,
                        bool create_directories = true);
  static void writeFile(const boost::filesystem::path &filename, const Json::Value &content,
                        bool create_directories = true);
  static void copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to);
  static std::string readFileFromArchive(std::istream &as, const std::string &filename);
  static void writeArchive(const std::map<std::string, std::string> &entries, std::ostream &as);
  static Json::Value getHardwareInfo();
  static Json::Value getNetworkInfo();
  static std::string getHostname();
  static std::string randomUuid();
  static sockaddr_storage ipGetSockaddr(int fd);
  static std::string ipDisplayName(const sockaddr_storage &saddr);
  static int ipPort(const sockaddr_storage &saddr);
  static int shell(const std::string &command, std::string *output, bool include_stderr = false);
  static boost::filesystem::path absolutePath(const boost::filesystem::path &root, const boost::filesystem::path &file);
  static void setSocketPort(sockaddr_storage *addr, in_port_t port);
  static std::vector<boost::filesystem::path> glob(const std::string &pat);
  static void createDirectories(const boost::filesystem::path &path, mode_t mode);
};

/**
 * RAII Temporary file creation
 */
class TemporaryFile {
 public:
  TemporaryFile(const std::string &hint = "file");
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
  TemporaryDirectory(const std::string &hint = "dir");
  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory operator=(TemporaryDirectory &) = delete;
  ~TemporaryDirectory();
  boost::filesystem::path Path() const;
  std::string PathString() const;
  boost::filesystem::path operator/(const boost::filesystem::path &subdir) const;

 private:
  boost::filesystem::path tmp_name_;
};

// helper template for C (mostly openssl) data structured
//   user should still take care about the order of destruction
//   by instantiating StructGuard<> in a right order.
//   BTW local variables are destructed in reverse order of instantiation
template <typename T>
using StructGuard = std::unique_ptr<T, void (*)(T *)>;

// helper object for RAII socket management
struct SocketCloser {
  void operator()(int *ptr) const {
    close(*ptr);
    delete ptr;
  }
};

using SocketHandle = std::unique_ptr<int, SocketCloser>;
bool operator<(const sockaddr_storage &left, const sockaddr_storage &right);  // required by std::map

// this is refference implementation of make_unique which is not yet included to C++11
namespace std {
template <class T>
struct _Unique_if {
  typedef unique_ptr<T> _Single_object;
};

template <class T>
struct _Unique_if<T[]> {
  typedef unique_ptr<T[]> _Unknown_bound;
};

template <class T, size_t N>
struct _Unique_if<T[N]> {
  typedef void _Known_bound;
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args &&... args) {
  return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
  typedef typename remove_extent<T>::type U;
  return unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args &&...) = delete;
}

#endif  // UTILS_H_
