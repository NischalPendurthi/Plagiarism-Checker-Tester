CXX = clang++
CXXWARNINGS = -Wall -Wshadow -Wuninitialized -Wtype-limits -Wno-unused-variable
CXXFLAGS = -std=gnu++20 -g -fsanitize=address,undefined $(CXXWARNINGS)
INCLUDEFLAGS = -I/usr/lib/llvm-18/include
LDFLAGS = -lclang -L/usr/lib/llvm-18/lib -rpath /usr/lib/llvm-18/lib -fuse-ld=lld

main: main0 main1 main3 main4 main5

main0: main_zero.cpp ../phase2/tokenizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
	rm -rf *.dSYM
	./main0

main1: main_one.cpp ../phase2/tokenizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
	rm -rf *.dSYM
	./main1


# main2: main_two.cpp ../phase2/tokenizer.cpp
# 	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
# 	rm -rf *.dSYM
# 	./main2

main3: main_three.cpp ../phase2/tokenizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
	rm -rf *.dSYM
	./main3

main4: main_four.cpp ../phase2/tokenizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
	rm -rf *.dSYM
	./main4

main5: main_five.cpp ../phase2/tokenizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) $^ -o $@
	rm -rf *.dSYM
	./main5

clean:
	rm -rf main0 main1 main2 main3 main4 main5 *.dSYM

.PHONY: main clean
