#include <map>

#include "httpclient.h"
#include "config.h"



class Uptane{
    public:
        enum ServiceType {
            Director = 0,
            Repo,
        };
        std::string getEndPointUrl(Uptane::ServiceType, const std::string & endpoint);
        struct Verified{
            std::string role;
            Json::Value data;
            unsigned long long old_version;
            unsigned long long new_version;
        };
    Uptane(const Config &config_in);
    void initService(Uptane::ServiceType service);
    Json::Value getJSON(Uptane::ServiceType service, const std::string &role);

    bool verify(Uptane::ServiceType service, const std::string& role,  Uptane::Verified& verified);

    bool verifyData(Uptane::ServiceType service, const std::string &role, const Json::Value &tuf_signed);
    private:
        
        struct Service{
            std::map<std::string, std::string> keys;
            //first argument im pair is thresold, second is version
            std::map<std::string, std::pair<unsigned int, unsigned long long > > roles;
        };
        
        std::map<ServiceType, Service> services;
        HttpClient *http;
        Config config;

        
};