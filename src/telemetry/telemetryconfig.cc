#include "telemetry/telemetryconfig.h"

#include <boost/log/trivial.hpp>

#include "utilities/config_utils.h"

void TelemetryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(report_network, "report_network", boost::log::trivial::trace, pt);
}

void TelemetryConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, report_network, "report_network");
}