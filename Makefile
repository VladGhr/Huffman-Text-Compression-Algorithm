CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
CXXNOEXE = -c
build: huffman.h classes.h compression.cpp
		$(CXX) $(CXXFLAGS) compression.cpp -o compression
		$(CXX) $(CXXFLAGS) decompression.cpp -o decompression
		$(CXX) $(CXXNOEXE) huffman.h
		$(CXX) $(CXXNOEXE) classes.h

run_comp: compression 
		./compression 

run_decomp: decompression
			./decompression

run: compression decompression
		./compression | ./decompression

clean:
		rm -f compression
		rm -f decompression
		rm -f huffman.o
		rm -f huffman.h.gch
		rm -f classes.o
		rm -f classes.h.gch
		rm -f compressedBinary.huff
		rm -f compressedPlain.huff
		rm -f decompressedPlain.txt

.PHONY:
		build clean