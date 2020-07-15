#include "ostree_repo.h"

#include "logging/logging.h"

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays)
OSTreeObject::ptr OSTreeRepo::GetObject(const uint8_t sha256[32], const OstreeObjectType type) const {
  return GetObject(OSTreeHash(sha256), type);
}

OSTreeObject::ptr OSTreeRepo::GetObject(const OSTreeHash hash, const OstreeObjectType type) const {
  otable::const_iterator obj_it = ObjectTable.find(hash);
  if (obj_it != ObjectTable.cend()) {
    return obj_it->second;
  }

  const std::map<OstreeObjectType, std::string> exts{{OstreeObjectType::OSTREE_OBJECT_TYPE_FILE, ".filez"},
                                                     {OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_TREE, ".dirtree"},
                                                     {OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META, ".dirmeta"},
                                                     {OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT, ".commit"}};
  const std::string objpath = hash.string().insert(2, 1, '/');
  OSTreeObject::ptr object;

  for (int i = 0; i < 3; ++i) {
    if (i > 0) {
      LOG_WARNING << "OSTree hash " << hash << " not found. Retrying (attempt " << i << " of 3)";
    }
    if (type != OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN) {
      if (CheckForObject(hash, objpath + exts.at(type), object)) {
        return object;
      }
    } else {
      for (auto it = exts.cbegin(); it != exts.cend(); ++it) {
        if (CheckForObject(hash, objpath + it->second, object)) {
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
