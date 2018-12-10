#ifndef GARAGE_COMMON_H_
#define GARAGE_COMMON_H_

/** Execution mode to run garage tools in. */
enum class RunMode {
  /** Default operation. Upload objects to server if necessary and relevant. */
  kDefault = 0,
  /** Dry run. Do not upload any objects. */
  kDryRun,
  /** Walk the entire tree (without uploading). Do not assume that if an object
   * exists, its parents must also exist. */
  kWalkTree,
  /** Walk the entire tree and upload any missing objects. Do not assume that if
   * an object exists, its parents must also exist. */
  kPushTree,
};

/** Types of OSTree objects, borrowed from libostree/ostree-core.h.
 *  Copied here to avoid a dependency. We do not currently handle types 5-7, and
 *  UNKNOWN is our own invention for pseudo-backwards compatibility.
 *
 * @OSTREE_OBJECT_TYPE_FILE: Content; regular file, symbolic link
 * @OSTREE_OBJECT_TYPE_DIR_TREE: List of children (trees or files), and metadata
 * @OSTREE_OBJECT_TYPE_DIR_META: Directory metadata
 * @OSTREE_OBJECT_TYPE_COMMIT: Toplevel object, refers to tree and dirmeta for root
 * @OSTREE_OBJECT_TYPE_TOMBSTONE_COMMIT: Toplevel object, refers to a deleted commit
 * @OSTREE_OBJECT_TYPE_COMMIT_META: Detached metadata for a commit
 * @OSTREE_OBJECT_TYPE_PAYLOAD_LINK: Symlink to a .file given its checksum on the payload only.
 *
 * Enumeration for core object types; %OSTREE_OBJECT_TYPE_FILE is for
 * content, the other types are metadata.
 */
enum class OstreeObjectType {
  OSTREE_OBJECT_TYPE_UNKNOWN = 0,
  OSTREE_OBJECT_TYPE_FILE = 1,             /* .file */
  OSTREE_OBJECT_TYPE_DIR_TREE = 2,         /* .dirtree */
  OSTREE_OBJECT_TYPE_DIR_META = 3,         /* .dirmeta */
  OSTREE_OBJECT_TYPE_COMMIT = 4,           /* .commit */
  OSTREE_OBJECT_TYPE_TOMBSTONE_COMMIT = 5, /* .commit-tombstone */
  OSTREE_OBJECT_TYPE_COMMIT_META = 6,      /* .commitmeta */
  OSTREE_OBJECT_TYPE_PAYLOAD_LINK = 7,     /* .payload-link */
};

#endif  // GARAGE_COMMON_H_
