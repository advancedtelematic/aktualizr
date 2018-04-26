#ifndef OPCUABRIDGE_DISCOVERYTYPES_H_
#define OPCUABRIDGE_DISCOVERYTYPES_H_

#include <array>

#include "opcuabridgeconfig.h"

namespace opcuabridge {
namespace discovery {

typedef std::array<unsigned char, OPCUA_DISCOVERY_SERVICE_DATASIZE> discovery_service_data_type;

enum EndPointServiceType { kUnknown, kNoLDS, kLDS };

}  // namespace discovery
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_DISCOVERYTYPES_H_
