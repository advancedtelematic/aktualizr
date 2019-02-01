#include <gtest/gtest.h>

#include "uptane/imagesrepository.h"

namespace Uptane {

TEST(ImagesRepository, DelegateTargets) {
  Targets toplevel;
  std::vector<Hash> hashes = {Hash("sha256", "deadbeef")};
  toplevel.targets.push_back(Target("t1", hashes, 0));
  toplevel.targets.push_back(Target("t2", hashes, 0));

  Targets delegate;
  delegate.targets.push_back(Target("abc/t1", hashes, 0));

  Role delegate_role = Role::Delegation("drole");
  toplevel.delegated_role_names_.insert(delegate_role.ToString());
  toplevel.paths_for_role_[delegate_role].push_back("abc/*");
  toplevel.terminating_role_[delegate_role] = true;

  ImagesRepository repo;
  repo.targets["targets"] = toplevel;
  repo.targets[delegate_role.ToString()] = delegate;

  auto t = repo.getTarget(Target("t2", hashes, 0));
  EXPECT_EQ("t2", t->filename());
  t = repo.getTarget(Target("bad", hashes, 0));
  EXPECT_EQ(nullptr, t);
  t = repo.getTarget(Target("abc/t1", hashes, 0));
  EXPECT_EQ("abc/t1", t->filename());
}

}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
