#include <iostream>
#include <algorithm>
#include <iterator>
#include <string>


int main(int argc, char **argv){
    (void) argc;
    (void) argv;

    std::string input;
    std::copy(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>(), std::back_inserter(input));

    (void) input; // TODO do something with this input like parse it or pass it to function

    return 0;
}
