#ifndef SOTA_CLIENT_TOOLS_REQUEST_POOL_H_
#define SOTA_CLIENT_TOOLS_REQUEST_POOL_H_

#include <list>

#include <curl/curl.h>

#include "garage_common.h"
#include "ostree_object.h"
#include "rate_controller.h"

class RequestPool {
 public:
  RequestPool(TreehubServer& server, int max_curl_requests, RunMode mode);
  ~RequestPool();
  void AddQuery(const OSTreeObject::ptr& request);
  void AddUpload(const OSTreeObject::ptr& request);
  void Abort() {
    stopped_ = true;
    query_queue_.clear();
    upload_queue_.clear();
  };
  bool is_idle() const { return query_queue_.empty() && upload_queue_.empty() && running_requests_ == 0; }
  bool is_stopped() const { return stopped_; }
  RunMode run_mode() const { return mode_; }

  /**
   * One iteration of request-listen loop, launches multiple requests, then
   * listens for the result.
   */
  void Loop();
  /**
   * The number of HEAD + PUT requests that have been sent to curl. This
   * includes requests that eventually returned 500 and get retried.
   */
  int put_requests_made() { return put_requests_made_; }
  int head_requests_made() { return head_requests_made_; }
  uintmax_t total_object_size() { return total_object_size_; }

 private:
  void LoopLaunch();  // launches multiple requests from the queues
  void LoopListen();  // listens to the result of launched requests

  RateController rate_controller_;
  int running_requests_;
  int head_requests_made_{0};
  int put_requests_made_{0};
  uintmax_t total_object_size_{0};
  TreehubServer& server_;
  CURLM* multi_;
  std::list<OSTreeObject::ptr> query_queue_;
  std::list<OSTreeObject::ptr> upload_queue_;
  RunMode mode_;
  bool stopped_;
};
// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_REQUEST_POOL_H_
