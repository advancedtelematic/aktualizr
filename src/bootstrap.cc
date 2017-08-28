#include "bootstrap.h"

#include <archive.h>
#include <archive_entry.h>
#include <boost/filesystem.hpp>

#include <stdio.h>
#include <fstream>
#include <sstream>

#include "crypto.h"
#include "logger.h"

Bootstrap::Bootstrap(const std::string& provision_path, const std::string& provision_password)
    : p12_str(""), ca(""), cert(""), pkey("") {
  if (!provision_path.empty()) {
    if (boost::filesystem::exists(provision_path)) {
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
        if (r != ARCHIVE_OK) LOGGER_LOG(LVL_error, "Error closing provision archive: " << provision_path);

        if (found) {
          p12_str = p12_stream.str();
          if (p12_str.empty()) throw std::runtime_error("Unable to parse bootstrap credentials");
          FILE* reg_p12 = fmemopen(const_cast<char*>(p12_str.c_str()), p12_str.size(), "rb");

          if (!reg_p12) throw std::runtime_error("Unable to parse bootstrap credentials");

          if (!Crypto::parseP12(reg_p12, provision_password, &pkey, &cert, &ca)) {
            fclose(reg_p12);
            throw std::runtime_error("Unable to parse bootstrap credentials");
          }
          fclose(reg_p12);
        } else {
          LOGGER_LOG(LVL_error, "autoprov_credentials.p12 not found in provision archive: " << provision_path);
          throw std::runtime_error("Unable to parse bootstrap credentials");
        }
      } else {
        LOGGER_LOG(LVL_error, "Could not read provision archive file, are you sure it is valid archive?");
        throw std::runtime_error("Unable to parse bootstrap credentials");
      }
    } else {
      LOGGER_LOG(LVL_error, "Provided provision archive '" << provision_path << "' does not exist!");
      throw std::runtime_error("Unable to parse bootstrap credentials");
    }
  } else {
    LOGGER_LOG(LVL_error, "Provided provision path is empty!");
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }
}

std::string Bootstrap::readServerUrl(const std::string& provision_path) {
  bool found = false;
  std::string url = "";
  std::stringstream url_stream;
  struct archive* a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  int r = archive_read_open_filename(a, provision_path.c_str(), 1024);
  if (r == ARCHIVE_OK) {
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      std::string filename(archive_entry_pathname(entry));
      if (filename == "autoprov.url") {
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
            url_stream.write(buff, size);
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
      url = url_stream.str();
    } else {
      LOGGER_LOG(LVL_error, "autoprov.url not found in provision archive: " << provision_path);
    }
  } else if (r != ARCHIVE_OK) {
    LOGGER_LOG(LVL_error, "Error opening provision archive: " << provision_path);
  }
  return url;
}
