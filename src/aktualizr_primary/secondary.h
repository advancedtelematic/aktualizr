#ifndef SECONDARY_H_
#define SECONDARY_H_

#include <boost/filesystem.hpp>

#include "libaktualizr/aktualizr.h"

namespace Primary {

void initSecondaries(Aktualizr& aktualizr, const boost::filesystem::path& config_file);

}  // namespace Primary

#endif  // SECONDARY_H_
