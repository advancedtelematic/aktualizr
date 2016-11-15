#ifndef SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_
#define SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_
#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>
#include <sstream>

#include "treehub_server.h"

class OSTreeRepo;
class RequestPool;

enum PresenceOnServer {
  OBJECT_STATE_UNKNOWN,
  OBJECT_PRESENT,
  OBJECT_MISSING,
  OBJECT_INPROGRESS,
  OBJECT_BREAKS_SERVER,
};

enum CurrentOp {
  OSTREE_OBJECT_UPLOADING,
  OSTREE_OBJECT_PRESENCE_CHECK,
};

class OSTreeObject : private boost::noncopyable {
 public:
  typedef boost::intrusive_ptr<OSTreeObject> ptr;
  OSTreeObject(const OSTreeRepo& repo, const std::string object_name);

  ~OSTreeObject();

  PresenceOnServer is_on_server() const { return is_on_server_; }
  CurrentOp operation() const { return current_operation_; }

  void CurlDone(CURLM* curl_multi_handle);

  void MakeTestRequest(const TreehubServer& push_target,
                       CURLM* curl_multi_handle);

  void Upload(const TreehubServer& push_target, CURLM* curl_multi_handle);

  void AddParent(OSTreeObject* parent,
                 std::list<OSTreeObject::ptr>::iterator parent_it);
  bool children_ready() { return children_.empty(); }
  void PopulateChildren();
  void QueryChildren(RequestPool& pool);
  void NotifyParents(RequestPool& pool);
  void ChildNotify(std::list<OSTreeObject::ptr>::iterator child_it);
  void LaunchNotify() { is_on_server_ = OBJECT_INPROGRESS; }

 private:
  typedef std::list<OSTreeObject::ptr>::iterator childiter;
  typedef std::pair<OSTreeObject*, childiter> parentref;

  void AppendChild(OSTreeObject::ptr child);
  std::string Url() const;

  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb,
                                  void* userp);

  friend void intrusive_ptr_add_ref(OSTreeObject*);
  friend void intrusive_ptr_release(OSTreeObject*);
  friend std::ostream& operator<<(std::ostream& stream, const OSTreeObject& o);

  const boost::filesystem::path file_path_;  // Full path to the object
  const std::string object_name_;            // OSTree name of the object
  const OSTreeRepo& repo_;
  int refcount_;  // refcounts and intrusive_ptr are used to simplify
                  // interaction with curl
  PresenceOnServer is_on_server_;
  CurrentOp current_operation_;

  std::stringstream http_response_;
  CURL* curl_handle_;
  struct curl_httppost* form_post_;
  std::list<parentref> parents_;
  std::list<OSTreeObject::ptr> children_;
};

OSTreeObject::ptr ostree_object_from_curl(CURL* curlhandle);

std::ostream& operator<<(std::ostream& stream, const OSTreeObject::ptr o);

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_OBJECT_H_
