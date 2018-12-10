#include "ostree_repo.h"

#include "logging/logging.h"

OSTreeObject::ptr OSTreeRepo::GetObject(const uint8_t sha256[32], const OstreeObjectType type) const {
  return GetObject(OSTreeHash(sha256), type);
}

OSTreeObject::ptr OSTreeRepo::GetObject(const OSTreeHash hash, const OstreeObjectType type) const {
  otable::const_iterator it;
  it = ObjectTable.find(hash);
  if (it != ObjectTable.end()) {
    return it->second;
  }

  const std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  const std::string objpath = hash.string().insert(2, 1, '/');
  OSTreeObject::ptr object;
  int ext_index = -1;
  switch (type) {
    case OstreeObjectType::OSTREE_OBJECT_TYPE_FILE:
      ext_index = 0;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_TREE:
      ext_index = 1;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META:
      ext_index = 2;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT:
      ext_index = 3;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN:
    default:
      break;
  }

  for (int i = 0; i < 3; ++i) {
    if (i > 0) {
      LOG_WARNING << "OSTree hash " << hash << " not found. Retrying (attempt " << i << " of 3)";
    }
    if (type != OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN) {
      if (CheckForObject(hash, objpath + exts[ext_index], object)) {
        return object;
      }
    } else {
      for (const std::string &ext : exts) {
        if (CheckForObject(hash, objpath + ext, object)) {
          return object;
        }
      }
    }
  }
  throw OSTreeObjectMissing(hash);
}

bool OSTreeRepo::CheckForObject(const OSTreeHash &hash, const std::string &path, OSTreeObject::ptr &object) const {
  if (FetchObject(std::string("objects/") + path)) {
    object = OSTreeObject::ptr(new OSTreeObject(*this, path));
    ObjectTable[hash] = object;
    LOG_DEBUG << "Fetched OSTree object " << path;
    return true;
  }
  return false;
}
