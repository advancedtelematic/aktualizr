#include <android/log.h>
#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>

namespace log = boost::log;

class android_log_sink : public log::sinks::basic_sink_backend<log::sinks::synchronized_feeding> {
 public:
  explicit android_log_sink() = default;

  void consume(log::record_view const& rec) {
    const auto& rec_message_attr = rec[log::aux::default_attribute_names::message()];
    int log_priority = android_LogPriority::ANDROID_LOG_VERBOSE +
                       rec[log::aux::default_attribute_names::severity()].extract_or_default(0);
    __android_log_write(log_priority, "aktualizr", rec_message_attr.extract_or_default(std::string("N/A")).c_str());
  }
};

void logger_init_sink(bool use_colors = false) {
  (void)use_colors;
  typedef log::sinks::synchronous_sink<android_log_sink> android_log_sink_t;
  log::core::get()->add_sink(boost::shared_ptr<android_log_sink_t>(new android_log_sink_t()));
}
