#include "types.h"

#include <stdexcept>

namespace data {
Json::Value Package::toJson() {
  Json::Value json;
  json["name"] = name;
  json["version"] = version;
  return json;
}

Package Package::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  Package package;
  package.name = json["name"].asString();
  package.version = json["version"].asString();
  return package;
}

Json::Value UpdateRequest::toJson() {
  Json::Value json;
  json["requestId"] = requestId;
  switch (status) {
    case Pending:
      json["status"] = "Pending";
      break;
    case InFlight:
      json["status"] = "InFlight";
      break;
    default:
      throw std::logic_error("Unknown value for status");
  }
  json["packageId"] = packageId.toJson();
  json["installPos"] = installPos;
  json["createdAt"] = createdAt;
  return json;
}

UpdateRequest UpdateRequest::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return fromJson(json);
}

UpdateRequest UpdateRequest::fromJson(const Json::Value& json) {
  UpdateRequest ur;
  ur.requestId = json["requestId"].asString();
  std::string status = json["status"].asString();
  if (status == "Pending") {
    ur.status = Pending;
  } else if (status == "InFlight") {
    ur.status = InFlight;
  } else {
    throw std::runtime_error("Failed to parse UpdateRequest");
  }
  ur.packageId = data::Package::fromJson(Json::FastWriter().write(json["packageId"]));
  ur.installPos = json["installPos"].asUInt();
  ur.createdAt = json["createdAt"].asString();
  return ur;
}

Json::Value UpdateAvailable::toJson() {
  Json::Value json;
  json["update_id"] = update_id;
  json["signature"] = signature;
  json["description"] = description;
  json["request_confirmation"] = request_confirmation;
  json["size"] = size;
  return json;
}

UpdateAvailable UpdateAvailable::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return fromJson(json);
}

UpdateAvailable UpdateAvailable::fromJson(const Json::Value& json) {
  UpdateAvailable ua;
  ua.update_id = json["update_id"].asString();
  ua.signature = json["signature"].asString();
  ua.description = json["description"].asString();
  ua.request_confirmation = json["request_confirmation"].asBool();
  ua.size = json["size"].asUInt();
  return ua;
}

Json::Value DownloadComplete::toJson() {
  Json::Value json;
  json["update_id"] = update_id;
  json["update_image"] = update_image;
  json["signature"] = signature;
  return json;
}

DownloadComplete DownloadComplete::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  DownloadComplete dc;
  dc.update_id = json["update_id"].asString();
  dc.update_image = json["update_image"].asString();
  dc.signature = json["signature"].asString();
  return dc;
}

Json::Value OperationResult::toJson() {
  Json::Value json;
  json["id"] = id;
  json["result_code"] = result_code;
  json["result_text"] = result_text;
  return json;
}

UpdateReport OperationResult::toReport() {
  UpdateReport report;
  report.update_id = id;
  report.operation_results.push_back(*this);
  return report;
}

OperationResult OperationResult::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  OperationResult operation_result;
  operation_result.id = json["id"].asString();
  operation_result.result_code = static_cast<UpdateResultCode>(json["result_code"].asUInt());
  operation_result.result_text = json["result_text"].asString();
  return operation_result;
}

OperationResult OperationResult::fromOutcome(const std::string& id, const InstallOutcome& outcome) {
  OperationResult operation_result;
  operation_result.id = id;
  operation_result.result_code = outcome.first;
  operation_result.result_text = outcome.second;
  return operation_result;
}

Json::Value UpdateReport::toJson() {
  Json::Value json;
  json["update_id"] = update_id;

  Json::Value operation_results_json(Json::arrayValue);
  for (std::vector<data::OperationResult>::iterator it = operation_results.begin(); it != operation_results.end();
       ++it) {
    operation_results_json.append(it->toJson());
  }
  json["operation_results"] = operation_results_json;
  return json;
}

UpdateReport UpdateReport::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  UpdateReport update_report;
  update_report.update_id = json["update_id"].asString();
  for (unsigned int index = 0; index < json["operation_results"].size(); ++index) {
    update_report.operation_results.push_back(
        OperationResult::fromJson(Json::FastWriter().write(json["operation_results"][index])));
  }
  return update_report;
}

Json::Value InstalledFirmware::toJson() {
  Json::Value json;
  json["module"] = module;
  json["firmware_id"] = firmware_id;
  json["last_modified"] = last_modified;
  return json;
}

InstalledFirmware InstalledFirmware::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  InstalledFirmware installed_firmware;
  installed_firmware.firmware_id = json["firmware_id"].asString();
  installed_firmware.module = json["module"].asString();
  installed_firmware.last_modified = json["last_modified"].asUInt();
  return installed_firmware;
}

Json::Value InstalledPackage::toJson() {
  Json::Value json;
  json["package_id"] = package_id;
  json["name"] = name;
  json["description"] = description;
  json["last_modified"] = last_modified;
  return json;
}

InstalledPackage InstalledPackage::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  InstalledPackage installed_package;
  installed_package.package_id = json["package_id"].asString();
  installed_package.name = json["name"].asString();
  installed_package.description = json["description"].asString();
  installed_package.last_modified = json["last_modified"].asUInt();
  return installed_package;
}

Json::Value InstalledSoftware::toJson() {
  Json::Value json;
  json["packages"] = Json::arrayValue;
  for (std::vector<InstalledPackage>::iterator it = packages.begin(); it != packages.end(); ++it) {
    json["packages"].append(it->toJson());
  }
  json["firmwares"] = Json::arrayValue;
  for (std::vector<InstalledFirmware>::iterator it = firmwares.begin(); it != firmwares.end(); ++it) {
    json["firmwares"].append(it->toJson());
  }
  return json;
}

InstalledSoftware InstalledSoftware::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<InstalledPackage> packages;

  for (unsigned int index = 0; index < json["packages"].size(); ++index) {
    packages.push_back(InstalledPackage::fromJson(Json::FastWriter().write(json["packages"][index])));
  }

  std::vector<InstalledFirmware> firmwares;
  for (unsigned int index = 0; index < json["firmwares"].size(); ++index) {
    firmwares.push_back(InstalledFirmware::fromJson(Json::FastWriter().write(json["firmwares"][index])));
  }
  InstalledSoftware installed_software;
  installed_software.packages = packages;
  installed_software.firmwares = firmwares;
  return installed_software;
}

PackageManagerCredentials::PackageManagerCredentials(const OnDiskCryptoKey& cryptokey)
    : tmp_ca_file("ostree-ca"), tmp_pkey_file("ostree-pkey"), tmp_cert_file("ostree-cert") {
  ca_file = cryptokey.getCa();
  if (ca_file.find("pkcs11:") != 0) {
    tmp_ca_file.PutContents(cryptokey.getCa());
    ca_file = tmp_ca_file.Path().string();
  }
  tmp_pkey_file.PutContents(cryptokey.getPkey());
  tmp_cert_file.PutContents(cryptokey.getCert());
}
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
