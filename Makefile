CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
CXXNOEXE = -c
build: huffman.h classes.h main.cpp
		$(CXX) $(CXXFLAGS) main.cpp -o main
		$(CXX) $(CXXNOEXE) huffman.h
		$(CXX) $(CXXNOEXE) classes.h

run: main
		./main

clean:
		rm -f main
		rm -f huffman.o
		rm -f huffman.h.gch
		rm -f classes.o
		rm -f classes.h.gch
		rm -f compressedBinary.huff
		rm -f compressedPlain.huff

.PHONY:
		build clean