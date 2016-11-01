#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

#include "ostree_repo.h"

using std::cout;
using std::string;
namespace fs = boost::filesystem;

bool OSTreeRepo::LooksValid() const {
  fs::path objects_dir(root_ + "/objects");
  fs::path refs_dir(root_ + "/refs");
  return fs::exists(objects_dir) && fs::exists(refs_dir);
}

void OSTreeRepo::FindAllObjects(std::list<OSTreeObject::ptr>* objects) const {
  fs::path objects_dir(root_ + "/objects");
  if (!fs::exists(objects_dir)) {
    cout << objects_dir << " not found\n";
    return;
  }

  fs::directory_iterator end_itr;  // one-past-end iterator

  for (fs::directory_iterator d1(objects_dir); d1 != end_itr; ++d1) {
    if (fs::is_directory(d1->status())) {
      fs::path fn = d1->path().filename();
      for (fs::directory_iterator d2(d1->path()); d2 != end_itr; ++d2) {
        if (fs::is_regular_file(d2->status())) {
          fs::path fn2 = fn / d2->path().filename();
          OSTreeObject::ptr obj(new OSTreeObject(root_, fn2.native()));
          objects->push_back(obj);
        }
      }
    }
  }
}

OSTreeRef OSTreeRepo::Ref(const string& refname) const {
  return OSTreeRef(root_, refname);
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
