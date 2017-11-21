#ifndef UTILS_H_
#define UTILS_H_

#include <boost/filesystem.hpp>
#include <string>

#include "json/json.h"

struct Utils {
  static std::string fromBase64(std::string);
  static std::string toBase64(const std::string &);
  static void hex2bin(const std::string hexstring, unsigned char *binout);
  static Json::Value parseJSON(const std::string &json_str);
  static Json::Value parseJSONFile(const boost::filesystem::path &filename);
  static std::string intToString(unsigned int val);
  static std::string genPrettyName();
  static std::string readFile(const std::string &filename);
  static void writeFile(const std::string &filename, const std::string &content);
  static void writeFile(const std::string &filename, const Json::Value &content);
  static void copyDir(const boost::filesystem::path &from, const boost::filesystem::path &to);
  static Json::Value getHardwareInfo();
  static std::string getHostname();
  static std::string randomUuid();
  static shell(const std::string& command, std::string* output);
};

/**
 * RAII Temporary file creation
 */
class TemporaryFile : boost::noncopyable {
 public:
  TemporaryFile(const std::string &hint = "");
  ~TemporaryFile();
  void PutContents(const std::string &contents);
  boost::filesystem::path Path() const;
  std::string PathString() const;

 private:
  boost::filesystem::path tmp_name_;
};

#endif  // UTILS_H_
