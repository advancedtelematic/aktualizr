CXX = g++
CXX_FLAGS = -Werror -Wall

%: %.cpp
	$(CXX) $(CXX_FLAGS) $^ -o $@

default: hello

test: hello
	./hello

clean:
	rm -f hello

.PHONY: test
