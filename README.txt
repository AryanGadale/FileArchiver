FILE ARCHIVER AND COMPRESSOR - MID-SEM ARCHIVE VERSION

C++11 compatible with older MinGW/G++ compilers.

Compile:
g++ -std=c++11 -O2 main.cpp -o FileArchiver

Run:
.\FileArchiver.exe

Choose:
1 = Compress a file to a .huff archive
2 = Extract a .huff archive
3 = Complete demo using sample.txt

Complete demo creates:
sample.huff
sample_extracted.txt

The custom .huff archive stores Huffman frequency metadata and packed bits.
For small files the archive can be larger than the original because of
metadata overhead; larger/repetitive files demonstrate the compression
benefit more clearly.
