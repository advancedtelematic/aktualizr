#include <gtest/gtest.h>

#include <atomic>

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sig_handler.h"

TEST(SigHandler, Catch) {
  pid_t child_pid;
  int pipefd[2];

  ASSERT_EQ(pipe(pipefd), 0);
  if ((child_pid = fork()) == 0) {
    // child
    std::atomic<bool> do_exit{false};

    close(pipefd[0]);

    SigHandler::get().start([&do_exit, pipefd]() {
      // child exited here
      close(pipefd[1]);
      do_exit.store(true);
    });
    SigHandler::signal(SIGINT);

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

    // wait for the child to be ready
    char b = 0;
    EXPECT_EQ(read(pipefd[0], &b, 1), 1);
    EXPECT_EQ(b, 'r');

    // kill the child
    kill(child_pid, SIGINT);

    // wait for child to exit
    int status;
    waitpid(child_pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
