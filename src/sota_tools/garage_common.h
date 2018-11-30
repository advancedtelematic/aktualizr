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
};

#endif  // GARAGE_COMMON_H_
