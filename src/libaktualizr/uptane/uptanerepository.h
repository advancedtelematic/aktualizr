#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include "fetcher.h"

class INvStorage;

namespace Uptane {

class RepositoryCommon {
 public:
  RepositoryCommon(RepositoryType type_in) : type{type_in} {}
  virtual ~RepositoryCommon() = default;
  bool initRoot(const std::string &root_raw);
  bool verifyRoot(const std::string &root_raw);
  int rootVersion() { return root.version(); }
  bool rootExpired() { return root.isExpired(TimeStamp::Now()); }
  virtual bool updateMeta(INvStorage &storage, const IMetadataFetcher &fetcher) = 0;
  Exception getLastException() const { return last_exception; }

 protected:
  void resetRoot();
  bool updateRoot(INvStorage &storage, const IMetadataFetcher &fetcher, RepositoryType repo_type);

  static const int64_t kMaxRotations = 1000;

  Root root;
  RepositoryType type;
  Exception last_exception{"", ""};
};
}  // namespace Uptane

#endif
