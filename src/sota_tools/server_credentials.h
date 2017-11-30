#ifndef SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_
#define SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_

#include <string>

enum AuthMethod { AUTH_NONE = 0, AUTH_BASIC, OAUTH2, CERT };

class ServerCredentials{
    public:
        ServerCredentials(const std::string &creds);
        bool CanSignOffline() const;
        AuthMethod GetMethod() const {return method_; };
        std::string GetClientCert() const { return client_cert_; };
        std::string GetClientKey() const { return client_key_; };
        std::string GetRootCert() const { return root_cert_; };
        std::string GetRepoUrl() const { return repo_url_; };
        std::string GetAuthUser() const { return auth_user_; };
        std::string GetAuthPassword() const { return auth_password_; };
        std::string GetAuthServer() const { return auth_server_; };
        std::string GetOSTreeServer() const { return ostree_server_; };
        std::string GetClientId() const { return client_id_; };
        std::string GetClientSecret() const { return client_secret_; };


    private:
        AuthMethod method_;
        std::string client_cert_;
        std::string client_key_;
        std::string root_cert_;
        std::string repo_url_;
        std::string auth_user_;
        std::string auth_password_;
        std::string auth_server_;
        std::string ostree_server_;
        std::string client_id_;
        std::string client_secret_;
        std::string credentials_path_;


};

#endif // SOTA_CLIENT_TOOLS_SERVER_CREDENTIALS_H_