#ifndef DIRECTOR_REPO_H_
#define DIRECTOR_REPO_H_

#include "repo.h"

class DirectorRepo : public Repo {
 public:
  DirectorRepo(boost::filesystem::path path, const std::string &expires, std::string correlation_id)
      : Repo(Uptane::RepositoryType::Director(), std::move(path), expires, std::move(correlation_id)) {}
  void addTarget(const std::string &target_name, const Json::Value &target, const std::string &hardware_id,
                 const std::string &ecu_serial);
  void signTargets();
  void emptyTargets();
  void oldTargets();
};

#endif  // DIRECTOR_REPO_H_
