#ifndef MAIN
#define MAIN

#include <iostream>
#include "compression.h"
#include "decompression.h"

/*//////////////////////////////////

TODO PARAMETRIZEAZA CITIREA DIN FISIER

MODULARIZEAZA FUNCTIONALITATEA DE COMPRESIE SI DECOMPRESIE

///////////////////////////////////*/

int main(int argc, char **argv) {


    if(argc < 2) {
        cerr << "Provide a file for compression";
    }

    pair<string, string> compressed_file_and_original_extension;

    compressed_file = compress(argv[1]);
    

}
#endif