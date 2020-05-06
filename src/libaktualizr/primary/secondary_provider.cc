#include "secondary_provider.h"

bool SecondaryProvider::getMetadata(Uptane::MetaBundle* meta_bundle, const Uptane::Target& target) const {
  std::string root;
  std::string timestamp;
  std::string snapshot;
  std::string targets;

  if (!getDirectorMetadata(&root, &targets)) {
    return false;
  }
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()), root);
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()), targets);

  if (!getImageRepoMetadata(&root, &timestamp, &snapshot, &targets)) {
    return false;
  }
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()), root);
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()), timestamp);
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()), snapshot);
  meta_bundle->emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets()), targets);

  // TODO: Support delegations for Secondaries. This is the purpose of providing
  // the desired Target.
  (void)target;

  return true;
}

bool SecondaryProvider::getDirectorMetadata(std::string* root, std::string* targets) const {
  if (!storage_->loadLatestRoot(root, Uptane::RepositoryType::Director())) {
    LOG_ERROR << "No Director Root metadata to send";
    return false;
  }
  if (!storage_->loadNonRoot(targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
    LOG_ERROR << "No Director Targets metadata to send";
    return false;
  }
  return true;
}

bool SecondaryProvider::getImageRepoMetadata(std::string* root, std::string* timestamp, std::string* snapshot,
                                             std::string* targets) const {
  if (!storage_->loadLatestRoot(root, Uptane::RepositoryType::Image())) {
    LOG_ERROR << "No Image repo Root metadata to send";
    return false;
  }
  if (!storage_->loadNonRoot(timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp())) {
    LOG_ERROR << "No Image repo Timestamp metadata to send";
    return false;
  }
  if (!storage_->loadNonRoot(snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot())) {
    LOG_ERROR << "No Image repo Snapshot metadata to send";
    return false;
  }
  if (!storage_->loadNonRoot(targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets())) {
    LOG_ERROR << "No Image repo Targets metadata to send";
    return false;
  }
  return true;
}

std::string SecondaryProvider::getTreehubCredentials() const {
  if (config_.tls.pkey_source != CryptoSource::kFile || config_.tls.cert_source != CryptoSource::kFile ||
      config_.tls.ca_source != CryptoSource::kFile) {
    LOG_ERROR << "Cannot send OSTree update to a Secondary when not using file as credential sources";
    return "";
  }
  std::string ca, cert, pkey;
  if (!storage_->loadTlsCreds(&ca, &cert, &pkey)) {
    LOG_ERROR << "Could not load TLS credentials from storage";
    return "";
  }

  const std::string treehub_url = config_.pacman.ostree_server;
  std::map<std::string, std::string> archive_map = {
      {"ca.pem", ca}, {"client.pem", cert}, {"pkey.pem", pkey}, {"server.url", treehub_url}};

  try {
    std::stringstream as;
    Utils::writeArchive(archive_map, as);

    return as.str();
  } catch (std::runtime_error& exc) {
    LOG_ERROR << "Could not create credentials archive: " << exc.what();
    return "";
  }
}

std::unique_ptr<StorageTargetRHandle> SecondaryProvider::getTargetFileHandle(const Uptane::Target& target) const {
  return storage_->openTargetFile(target);
}
