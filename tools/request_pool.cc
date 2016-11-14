#include "request_pool.h"
#include <exception>

RequestPool::RequestPool(const TreehubServer& server, int max_requests)
    : max_requests_(max_requests),
      running_requests_(0),
      server_(server),
      stopped_(false) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  multi_ = curl_multi_init();
}

RequestPool::~RequestPool() {
  Abort();

  while (!is_idle()) LoopListen();

  curl_multi_cleanup(multi_);
  curl_global_cleanup();
}

void RequestPool::LoopLaunch() {
  while (running_requests_ < max_requests_ &&
         (!query_queue_.empty() || !upload_queue_.empty())) {
    OSTreeObject::ptr cur;

    // Queries first, uploads second
    if (query_queue_.empty()) {
      cur = upload_queue_.front();
      upload_queue_.pop_front();
      cur->Upload(server_, multi_);
    } else {
      cur = query_queue_.front();
      query_queue_.pop_front();
      cur->MakeTestRequest(server_, multi_);
    }

    running_requests_++;
  }
}

void RequestPool::LoopListen() {
  // Poll for IO
  fd_set fdread, fdwrite, fdexcept;
  int maxfd = 0;
  FD_ZERO(&fdread);
  FD_ZERO(&fdwrite);
  FD_ZERO(&fdexcept);
  long timeoutms = 0;
  curl_multi_timeout(multi_, &timeoutms);
  struct timeval timeout;
  timeout.tv_sec = timeoutms / 1000;
  timeout.tv_usec = 1000 * (timeoutms % 1000);
  curl_multi_fdset(multi_, &fdread, &fdwrite, &fdexcept, &maxfd);
  select(maxfd + 1, &fdread, &fdwrite, &fdexcept,
         timeoutms == -1 ? NULL : &timeout);

  // Ask curl to handle IO
  CURLMcode mc = curl_multi_perform(multi_, &running_requests_);

  if (mc != CURLM_OK) throw std::runtime_error("curl_multi failed with error");

  // Deal with any completed requests
  int msgs_in_queue;
  do {
    CURLMsg* msg = curl_multi_info_read(multi_, &msgs_in_queue);
    if (msg && msg->msg == CURLMSG_DONE) {
      OSTreeObject::ptr h = ostree_object_from_curl(msg->easy_handle);
      h->CurlDone(multi_);
      switch (h->operation()) {
        case OSTREE_OBJECT_UPLOADING:
          upload_cb_(*this, h);
          break;
        case OSTREE_OBJECT_PRESENCE_CHECK:
          query_cb_(*this, h);
          break;
        default:
          throw std::runtime_error("Unexpected current operation on an object");
          break;
      }
    }
  } while (msgs_in_queue > 0);
}

void RequestPool::Loop() {
  LoopLaunch();
  LoopListen();
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
