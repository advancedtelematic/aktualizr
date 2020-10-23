#ifndef SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_
#define SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_

#include <string>

#include <boost/filesystem.hpp>

enum class AuthMethod { kNone = 0, kBasic, kOauth2, kTls };

class BadCredentialsContent : public std::runtime_error {
 public:
  explicit BadCredentialsContent(const std::string &what_arg) : std::runtime_error(what_arg.c_str()) {}
};

class BadCredentialsJson : public std::runtime_error {
 public:
  explicit BadCredentialsJson(const std::string &what_arg) : std::runtime_error(what_arg.c_str()) {}
};

class BadCredentialsArchive : public std::runtime_error {
 public:
  explicit BadCredentialsArchive(const std::string &what_arg) : std::runtime_error(what_arg.c_str()) {}
};

class ServerCredentials {
 public:
  explicit ServerCredentials(const boost::filesystem::path &credentials_path);
  bool CanSignOffline() const;
  AuthMethod GetMethod() const { return method_; };
  std::string GetClientP12() const { return client_p12_; };
  std::string GetRepoUrl() const { return repo_url_; };
  std::string GetAuthUser() const { return auth_user_; };
  std::string GetAuthPassword() const { return auth_password_; };
  std::string GetAuthServer() const { return auth_server_; };
  std::string GetOSTreeServer() const { return ostree_server_; };
  std::string GetClientId() const { return client_id_; };
  std::string GetClientSecret() const { return client_secret_; };
  std::string GetScope() const { return scope_; };

  /**
   * Path to the original credentials.zip on disk.  Needed to hand off to
   * garage-sign executable.
   */
  boost::filesystem::path GetPathOnDisk() const { return credentials_path_; }

 private:
  AuthMethod method_;
  std::string client_p12_;
  std::string repo_url_;
  std::string auth_user_;
  std::string auth_password_;
  std::string auth_server_;
  std::string ostree_server_;
  std::string client_id_;
  std::string client_secret_;
  std::string scope_;
  boost::filesystem::path credentials_path_;
};

#endif  // SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_
