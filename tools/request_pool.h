#ifndef SOTA_CLIENT_TOOLS_REQUEST_POOL_H_
#define SOTA_CLIENT_TOOLS_REQUEST_POOL_H_

#include <curl/curl.h>
#include <list>
#include "ostree_object.h"

class RequestPool {
 public:
  typedef void (*requestCallBack)(RequestPool&, OSTreeObject::ptr);

  RequestPool(const TreehubServer& server, int poolsize);
  ~RequestPool();
  void add_query(OSTreeObject::ptr request) { query_queue_.push_back(request); }
  void add_upload(OSTreeObject::ptr request) {
    upload_queue_.push_back(request);
  }
  void abort() {
    query_queue_.clear();
    upload_queue_.clear();
  };
  bool is_idle() {
    return query_queue_.empty() && upload_queue_.empty() &&
           running_requests_ == 0;
  }
  void loop();  // one iteration of request-listen loop, launches up to
                // max_requests_ requests, then listens for the result
  void on_query(requestCallBack cb) { query_cb_ = cb; }
  void on_upload(requestCallBack cb) { upload_cb_ = cb; }

 private:
  int max_requests_;
  int running_requests_;
  const TreehubServer& server_;
  CURLM* multi_;
  std::list<OSTreeObject::ptr> query_queue_;
  std::list<OSTreeObject::ptr> upload_queue_;
  requestCallBack query_cb_;
  requestCallBack upload_cb_;

  void loop_launch();  // launches up to max_requests_ requests from the queues
  void loop_listen();  // listens to the result of launched requests
};
// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_REQUEST_POOL_H_
