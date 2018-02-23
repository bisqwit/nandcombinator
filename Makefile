#CXX=clang++-5.0
CXX=g++-7.1
#CXX=g++-7.1 -fsanitize=address -g
CXXFLAGS=-std=c++17 -Wall -Wextra -Ofast -fopenmp -march=native

all: gatebuilder nandcombinator

gatebuilder: gatebuilder.cc
	$(CXX) -o $@ $^ $(CXXFLAGS)
nandcombinator: nandcombinator.cc
	$(CXX) -o $@ $^ $(CXXFLAGS)
