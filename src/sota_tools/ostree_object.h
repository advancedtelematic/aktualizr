#ifndef SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_
#define SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_

#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>
#include <chrono>
#include <iostream>
#include <sstream>

#include "treehub_server.h"

class OSTreeRepo;
class RequestPool;

enum PresenceOnServer { OBJECT_STATE_UNKNOWN, OBJECT_PRESENT, OBJECT_MISSING, OBJECT_INPROGRESS };

enum CurrentOp {
  OSTREE_OBJECT_UPLOADING,
  OSTREE_OBJECT_PRESENCE_CHECK,
};

/**
 * Broad categories for server response codes.
 * There is no category for a permanent failure at the moment: we are able to
 * detect a failure that is definitely permanent.
 */
enum class ServerResponse { kNoResponse, kOk, kTemporaryFailure };

class OSTreeObject {
 public:
  using ptr = boost::intrusive_ptr<OSTreeObject>;
  OSTreeObject(const OSTreeRepo& repo, const std::string& object_name);
  OSTreeObject(const OSTreeObject&) = delete;
  OSTreeObject operator=(const OSTreeObject&) = delete;

  ~OSTreeObject();

  PresenceOnServer is_on_server() const { return is_on_server_; }
  CurrentOp operation() const { return current_operation_; }

  void CurlDone(CURLM* curl_multi_handle, RequestPool& pool);

  void MakeTestRequest(const TreehubServer& push_target, CURLM* curl_multi_handle);

  void Upload(const TreehubServer& push_target, CURLM* curl_multi_handle, bool dryrun);

  void AddParent(OSTreeObject* parent, std::list<OSTreeObject::ptr>::iterator parent_it);
  bool children_ready() { return children_.empty(); }
  void PopulateChildren();
  void QueryChildren(RequestPool& pool);
  void NotifyParents(RequestPool& pool);
  void ChildNotify(std::list<OSTreeObject::ptr>::iterator child_it);
  void LaunchNotify() { is_on_server_ = OBJECT_INPROGRESS; }

  std::chrono::steady_clock::time_point RequestStartTime() const { return request_start_time_; }

  ServerResponse LastOperationResult() const { return last_operation_result_; }

 private:
  using childiter = std::list<OSTreeObject::ptr>::iterator;
  typedef std::pair<OSTreeObject*, childiter> parentref;

  void AppendChild(const OSTreeObject::ptr& child);
  std::string Url() const;

  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);

  friend void intrusive_ptr_add_ref(OSTreeObject* /*h*/);
  friend void intrusive_ptr_release(OSTreeObject* /*h*/);
  friend std::ostream& operator<<(std::ostream& stream, const OSTreeObject& o);

  const boost::filesystem::path file_path_;  // Full path to the object
  const std::string object_name_;            // OSTree name of the object
  const OSTreeRepo& repo_;
  int refcount_;  // refcounts and intrusive_ptr are used to simplify
                  // interaction with curl
  PresenceOnServer is_on_server_;
  CurrentOp current_operation_{};

  std::stringstream http_response_;
  CURL* curl_handle_;
  struct curl_httppost* form_post_;
  std::list<parentref> parents_;
  std::list<OSTreeObject::ptr> children_;

  std::chrono::steady_clock::time_point request_start_time_;
  ServerResponse last_operation_result_{ServerResponse::kNoResponse};
};

OSTreeObject::ptr ostree_object_from_curl(CURL* curlhandle);

std::ostream& operator<<(std::ostream& stream, const OSTreeObject::ptr& o);

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_
