#include "update_agent_ostree.h"

#include "package_manager/ostreemanager.h"

// TODO: consider moving this and SotaUptaneClient::secondaryTreehubCredentials() to encapsulate them in one place that
// is shared between IP Secondary's component
static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert, std::string* pkey,
                                      std::string* treehub_server);

bool OstreeUpdateAgent::isTargetSupported(const Uptane::Target& target) const { return target.IsOstree(); }

bool OstreeUpdateAgent::getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const {
  bool result = false;
  try {
    installed_image_info.len = 0;
    installed_image_info.hash = ostreePackMan_->getCurrentHash();

    // TODO(OTA-4545): consider more elegant way of storing currently installed target name
    // usage of the SQLStorage and ostree implementions aimed for Primary is
    // a quite overhead for Secondary
    auto currently_installed_target = ostreePackMan_->getCurrent();
    if (!currently_installed_target.IsValid()) {
      // This is the policy on a target image name in case of ostree
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

bool OstreeUpdateAgent::downloadTargetRev(const Uptane::Target& target, const std::string& treehub_tls_creds) {
  std::string treehub_server;
  bool download_result = false;

  try {
    std::string ca, cert, pkey, server_url;
    extractCredentialsArchive(treehub_tls_creds, &ca, &cert, &pkey, &server_url);
    // TODO: why are qe loading this credentials at all ?
    keyMngr_->loadKeys(&pkey, &cert, &ca);
    boost::trim(server_url);
    treehub_server = server_url;
  } catch (std::runtime_error& exc) {
    LOG_ERROR << exc.what();
    return false;
  }

  auto install_res = OstreeManager::pull(sysrootPath_, treehub_server, *keyMngr_, target);

  switch (install_res.result_code.num_code) {
    case data::ResultCode::Numeric::kOk: {
      LOG_INFO << "The target revision has been successfully downloaded: " << target.sha256Hash();
      download_result = true;
      break;
    }
    case data::ResultCode::Numeric::kAlreadyProcessed: {
      LOG_INFO << "The target revision is already present on the local ostree repo: " << target.sha256Hash();
      download_result = true;
      break;
    }
    default: {
      LOG_ERROR << "Failed to download the target revision: " << target.sha256Hash() << " ( "
                << install_res.result_code.toString() << " ): " << install_res.description;
    }
  }

  return download_result;
}

data::ResultCode::Numeric OstreeUpdateAgent::install(const Uptane::Target& target) {
  return (ostreePackMan_->install(target)).result_code.num_code;
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
