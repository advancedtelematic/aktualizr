#ifndef LT_PROVISION_H
#define LT_PROVISION_H

#include <boost/filesystem.hpp>

void mkDevices(const boost::filesystem::path& dst_dir, const boost::filesystem::path& bootstrapCredentials,
               const std::string& gw_uri, const size_t parallelism, const unsigned int nr, const unsigned int rate);

#endif
