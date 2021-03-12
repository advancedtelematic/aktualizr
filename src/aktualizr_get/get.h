#ifndef AKTUALIZR_GET_HELPERS
#define AKTUALIZR_GET_HELPERS

#include "libaktualizr/config.h"
#include "storage/invstorage.h"

std::string aktualizrGet(Config &config, const std::string &url, const std::vector<std::string> &headers,
                         StorageClient storage_client = StorageClient::kUptane);

#endif  // AKTUALIZR_GET_HELPERS
