#include <iostream>
#include "utils.h"

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::cout << Utils::genPrettyName() << std::endl;

  return 0;
}
