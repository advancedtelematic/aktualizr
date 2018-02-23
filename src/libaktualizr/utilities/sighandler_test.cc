#include <gtest/gtest.h>

#include <atomic>

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sig_handler.h"

TEST(SigHandler, MaskForNSeconds) {
  pid_t child_pid;
  int pipefd[2];

  ASSERT_EQ(pipe(pipefd), 0);
  if ((child_pid = fork()) == 0) {
    std::atomic<bool> do_exit{false};

    // child
    close(pipefd[0]);

    SigHandler::get().start([&do_exit, pipefd]() {
      // child exited here
      close(pipefd[1]);
      do_exit.store(true);
    });

    // mask signals for 1 second
    SigHandler::get().mask(1);

    // signal that we're ready to be killed
    if (write(pipefd[1], "r", 1) != 1) {
      exit(1);
    }

    while (true) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(20));
      if (do_exit.load()) {
        break;
      }
    }
    exit(0);
  } else {
    // parent
    close(pipefd[1]);

    // wait for the child to be ready (SIGINT masked)
    char b = 0;
    EXPECT_EQ(read(pipefd[0], &b, 1), 1);
    EXPECT_EQ(b, 'r');

    // kill the child
    struct timeval time {};
    EXPECT_EQ(gettimeofday(&time, nullptr), 0);

    kill(child_pid, SIGINT);

    // wait for child to exit
    int status;
    waitpid(child_pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));

    struct timeval time_new {};
    EXPECT_EQ(gettimeofday(&time_new, nullptr), 0);

    // check that the time to kill is within reasonable bounds
    int64_t elapsed = ((time_new.tv_sec * 1000000l) + time_new.tv_usec) - ((time.tv_sec * 1000000l) + time.tv_usec);
    EXPECT_GE(elapsed, 1 * 1000000lu);
    EXPECT_LT(elapsed, 2 * 1000000lu);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
