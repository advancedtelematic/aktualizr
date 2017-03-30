#include <map>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "ostree.h"

typedef boost::archive::iterators::base64_from_binary<
    boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8> >
    base64_text;

class SotaUptaneClient {
 public:
  enum ServiceType {
    Director = 0,
    Repo,
  };
  std::string getEndPointUrl(SotaUptaneClient::ServiceType, const std::string &endpoint);
  struct Verified {
    bool is_new() { return new_version > old_version; }
    std::string role;
    Json::Value data;
    unsigned long long old_version;
    unsigned long long new_version;
  };

  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in);
  bool verifyData(SotaUptaneClient::ServiceType service, const std::string &role, const Json::Value &tuf_signed);
  void initService(SotaUptaneClient::ServiceType service);
  bool verify(SotaUptaneClient::ServiceType service, const std::string &role, SotaUptaneClient::Verified &verified, bool force_fetch);
  void putManfiest(SotaUptaneClient::ServiceType service);
  Json::Value getJSON(SotaUptaneClient::ServiceType service, const std::string &role, bool force_fetch);
  std::string sign(const Json::Value &in_data, Json::Value &out_data);
  void OstreeInstall(std::vector<OstreePackage> packages);
  std::vector<OstreePackage> getAvailableUpdates();
  bool deviceRegister();
  bool ecuRegister();
  bool authenticate();
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  struct Service {
    std::map<std::string, std::string> keys;
    // first argument in pair is thresold, second is version
    std::map<std::string, std::pair<unsigned int, unsigned long long> > roles;
  };

  std::map<ServiceType, Service> services;
  HttpClient *http;
  Config config;
  event::Channel *events_channel;

  std::vector<Json::Value> ecu_versions;
  bool processing;
  bool was_error;
};