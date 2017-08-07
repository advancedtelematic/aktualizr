#include "bootstrap.h"

#include <archive.h>
#include <archive_entry.h>
#include <boost/filesystem.hpp>

#include <stdio.h>
#include <fstream>
#include <sstream>

#include "logger.h"

Bootstrap::Bootstrap(const std::string& provision_path_in)
    : provision_path(provision_path_in), p12_str(""), pkey_path(""), cert_path(""), ca_path("") {
  if (!provision_path.empty()) {
    if (boost::filesystem::exists(provision_path)) {
      boost::filesystem::path provision_directory(provision_path);
      provision_directory.remove_filename();

      bool found = false;
      std::stringstream p12_stream;
      struct archive* a = archive_read_new();
      archive_read_support_filter_all(a);
      archive_read_support_format_all(a);
      int r = archive_read_open_filename(a, provision_path.c_str(), 20 * 512);
      if (r == ARCHIVE_OK) {
        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
          std::string filename(archive_entry_pathname(entry));
          if (filename == "autoprov_credentials.p12") {
            const char* buff;
            size_t size;
            int64_t offset;

            for (;;) {
              r = archive_read_data_block(a, (const void**)&buff, &size, &offset);
              if (r == ARCHIVE_EOF) {
                break;
              } else if (r != ARCHIVE_OK) {
                LOGGER_LOG(LVL_error, "Error reading provision archive: " << archive_error_string(a));
              } else if (size > 0 && buff != NULL) {
                p12_stream.write(buff, size);
              }
            }
            found = true;
          } else {
            archive_read_data_skip(a);
          }
        }
        r = archive_read_free(a);
        if (r != ARCHIVE_OK) {
          LOGGER_LOG(LVL_error, "Error closing provision archive: " << provision_path);
        }
        if (found) {
          p12_str = p12_stream.str();

          std::string pkey_filename = boost::filesystem::unique_path().string();
          pkey_path = (provision_directory / pkey_filename).string();

          std::string cert_filename = boost::filesystem::unique_path().string();
          cert_path = (provision_directory / cert_filename).string();

          std::string ca_filename = boost::filesystem::unique_path().string();
          ca_path = (provision_directory / ca_filename).string();
        } else {
          LOGGER_LOG(LVL_error, "autoprov_credentials.p12 not found in provision archive: " << provision_path);
        }
      } else {
        LOGGER_LOG(LVL_error, "Could not read provision archive file, are you sure it is valid archive?");
      }
    } else {
      LOGGER_LOG(LVL_error, "Provided provision archive '" << provision_path << "' does not exist!");
    }
  } else {
    LOGGER_LOG(LVL_error, "Provided provision path is empty!");
  }
}

Bootstrap::~Bootstrap() {
  if (!pkey_path.empty()) {
    if (boost::filesystem::exists(pkey_path)) {
      int r = remove(pkey_path.c_str());
      if (r != 0) {
        LOGGER_LOG(LVL_error, "Unable to remove provision file: '" << pkey_path);
      }
    }
  }
  if (!cert_path.empty()) {
    if (boost::filesystem::exists(cert_path)) {
      int r = remove(cert_path.c_str());
      if (r != 0) {
        LOGGER_LOG(LVL_error, "Unable to remove provision file: '" << cert_path);
      }
    }
  }
  if (!ca_path.empty()) {
    if (boost::filesystem::exists(ca_path)) {
      int r = remove(ca_path.c_str());
      if (r != 0) {
        LOGGER_LOG(LVL_error, "Unable to remove provision file: '" << ca_path);
      }
    }
  }
}

std::string Bootstrap::getP12Str() const { return p12_str; }

std::string Bootstrap::getPkeyPath() const { return pkey_path; }

std::string Bootstrap::getCertPath() const { return cert_path; }

std::string Bootstrap::getCaPath() const { return ca_path; }
