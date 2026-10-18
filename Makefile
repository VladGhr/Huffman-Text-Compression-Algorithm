CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
CXXNOEXE = -c
build: huffman.cpp classes.h
		$(CXX) $(CXXFLAGS) huffman.cpp -o huffman
		$(CXX) $(CXXNOEXE) classes.h

run: huffman
		./huffman

clean:
		rm -f huffman
		rm -f classes.o
		rm -f classes.h.gch

.PHONY:
		build clean