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
  void AddQuery(OSTreeObject::ptr request) {
    request->LaunchNotify();
    query_queue_.push_back(request);
  }
  void AddUpload(OSTreeObject::ptr request) {
    request->LaunchNotify();
    upload_queue_.push_back(request);
  }
  void Abort() {
    stopped_ = true;
    query_queue_.clear();
    upload_queue_.clear();
  };
  bool is_idle() {
    return query_queue_.empty() && upload_queue_.empty() &&
           running_requests_ == 0;
  }
  bool is_stopped() { return stopped_; }
  void Loop();  // one iteration of request-listen loop, launches up to
                // max_requests_ requests, then listens for the result
  void OnQuery(requestCallBack cb) { query_cb_ = cb; }
  void OnUpload(requestCallBack cb) { upload_cb_ = cb; }

 private:
  void LoopLaunch();  // launches up to max_requests_ requests from the queues
  void LoopListen();  // listens to the result of launched requests

  int max_requests_;
  int running_requests_;
  const TreehubServer& server_;
  CURLM* multi_;
  std::list<OSTreeObject::ptr> query_queue_;
  std::list<OSTreeObject::ptr> upload_queue_;
  requestCallBack query_cb_;
  requestCallBack upload_cb_;
  bool stopped_;
};
// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_REQUEST_POOL_H_
