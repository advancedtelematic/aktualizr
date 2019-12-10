#ifndef TELEMETRY_TELEMETRY_CONFIG_H_
#define TELEMETRY_TELEMETRY_CONFIG_H_

#include <boost/property_tree/ptree_fwd.hpp>

struct TelemetryConfig {
  /**
   * Report device network information: IP address, hostname, MAC address
   */
  bool report_network{true};
  bool report_config{true};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // TELEMETRY_TELEMETRY_CONFIG_H_
