#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

using Uptane::Root;

Root::Root(const RepositoryType repo, const Json::Value &json, Root &root) : Root(repo, json) {
  root.UnpackSignedObject(repo, Role::Root(), json);
  this->Root::UnpackSignedObject(repo, Role::Root(), json);
}

Root::Root(const RepositoryType repo, const Json::Value &json) : MetaWithKeys(json), policy_(Policy::kCheck) {
  if (!json["signed"].isMember("keys")) {
    throw InvalidMetadata(repo, "root", "missing keys field");
  } else if (!json["signed"].isMember("roles")) {
    throw InvalidMetadata(repo, "root", "missing roles field");
  }

  const Json::Value keys = json["signed"]["keys"];
  ParseKeys(repo, keys);

  const Json::Value roles = json["signed"]["roles"];
  for (auto it = roles.begin(); it != roles.end(); it++) {
    const Role role = Role(it.key().asString());
    ParseRole(repo, it, role, "root");
  }
}

void Uptane::Root::UnpackSignedObject(const RepositoryType repo, const Role &role, const Json::Value &signed_object) {
  const std::string repository = repo;

  if (policy_ == Policy::kAcceptAll) {
    return;
  }
  if (policy_ == Policy::kRejectAll) {
    throw SecurityException(repository, "Root policy is Policy::kRejectAll");
  }
  assert(policy_ == Policy::kCheck);

  Uptane::MetaWithKeys::UnpackSignedObject(repo, role, signed_object);
}
