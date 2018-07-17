#ifndef REPORTQUEUE_H_
#define REPORTQUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <json/json.h>

#include "config/config.h"
#include "http/httpclient.h"
#include "logging/logging.h"

class ReportQueue {
 public:
  ReportQueue(const Config &config_in, std::shared_ptr<HttpInterface> http_client);
  ~ReportQueue();
  void run();
  void enqueue(std::unique_ptr<Json::Value> report);

 private:
  const Config &config;
  std::shared_ptr<HttpInterface> http;
  std::thread thread_;
  std::condition_variable cv_;
  std::mutex thread_mutex_;
  std::mutex queue_mutex_;
  std::queue<std::unique_ptr<Json::Value>> report_queue_;
  bool shutdown_;
};

#endif  // REPORTQUEUE_H_
