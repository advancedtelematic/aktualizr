#include "update_agent_ostree.h"

#include "package_manager/ostreemanager.h"

// TODO: consider moving this and SecondaryProvider::getTreehubCredentials to
// encapsulate them in one shared place if possible.
static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert, std::string* pkey,
                                      std::string* treehub_server);

// TODO(OTA-4939): Unify this with the check in
// SotaUptaneClient::getNewTargets() and make it more generic.
bool OstreeUpdateAgent::isTargetSupported(const Uptane::Target& target) const { return target.IsOstree(); }

bool OstreeUpdateAgent::getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const {
  bool result = false;
  try {
    installed_image_info.len = 0;
    installed_image_info.hash = ostreePackMan_->getCurrentHash();

    // TODO(OTA-4545): consider more elegant way of storing currently installed target name
    // usage of the SQLStorage and OSTree implementions aimed for Primary is
    // a quite overhead for Secondary
    auto currently_installed_target = ostreePackMan_->getCurrent();
    if (!currently_installed_target.IsValid()) {
      // This is the policy on a target image name in case of OSTree
      // The policy in followed and implied in meta-updater (garage-sign/push) and the backend
      // installed_image_info.name = _targetname_prefix + "-" + installed_image_info.hash;
      installed_image_info.name = targetname_prefix_ + "-" + installed_image_info.hash;
    } else {
      installed_image_info.name = currently_installed_target.filename();
    }

    result = true;
  } catch (const std::exception& exc) {
    LOG_ERROR << "Failed to get the currently installed revision: " << exc.what();
  }
  return result;
}

data::InstallationResult OstreeUpdateAgent::downloadTargetRev(const Uptane::Target& target,
                                                              const std::string& treehub_tls_creds) {
  std::string treehub_server;

  try {
    std::string ca;
    std::string cert;
    std::string pkey;
    std::string server_url;
    extractCredentialsArchive(treehub_tls_creds, &ca, &cert, &pkey, &server_url);
    keyMngr_->loadKeys(&pkey, &cert, &ca);
    boost::trim(server_url);
    treehub_server = server_url;
  } catch (std::runtime_error& exc) {
    LOG_ERROR << exc.what();
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    std::string("Error loading Treehub credentials: ") + exc.what());
  }

  auto result = OstreeManager::pull(sysrootPath_, treehub_server, *keyMngr_, target);

  switch (result.result_code.num_code) {
    case data::ResultCode::Numeric::kOk: {
      LOG_INFO << "The target revision has been successfully downloaded: " << target.sha256Hash();
      break;
    }
    case data::ResultCode::Numeric::kAlreadyProcessed: {
      LOG_INFO << "The target revision is already present on the local OSTree repo: " << target.sha256Hash();
      break;
    }
    default: {
      LOG_ERROR << "Failed to download the target revision: " << target.sha256Hash() << " ( "
                << result.result_code.toString() << " ): " << result.description;
    }
  }

  return result;
}

data::InstallationResult OstreeUpdateAgent::install(const Uptane::Target& target) {
  return ostreePackMan_->install(target);
}

void OstreeUpdateAgent::completeInstall() { ostreePackMan_->completeInstall(); }

data::InstallationResult OstreeUpdateAgent::applyPendingInstall(const Uptane::Target& target) {
  return ostreePackMan_->finalizeInstall(target);
}

void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert, std::string* pkey,
                               std::string* treehub_server) {
  {
    std::stringstream as(archive);
    *ca = Utils::readFileFromArchive(as, "ca.pem");
  }
  {
    std::stringstream as(archive);
    *cert = Utils::readFileFromArchive(as, "client.pem");
  }
  {
    std::stringstream as(archive);
    *pkey = Utils::readFileFromArchive(as, "pkey.pem");
  }
  {
    std::stringstream as(archive);
    *treehub_server = Utils::readFileFromArchive(as, "server.url", true);
  }
}
