#include <libaktualizr/telemetryconfig.h>
#include <libaktualizr/config_utils.h>

void TelemetryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(report_network, "report_network", pt);
  CopyFromConfig(report_config, "report_config", pt);
}

void TelemetryConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, report_network, "report_network");
  writeOption(out_stream, report_config, "report_config");
}
