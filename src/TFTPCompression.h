#ifndef TFTP_COMPRESSION_H
#define TFTP_COMPRESSION_H
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <cstring>

void readBinaryFile(const char* filename);
std::string addBinary(std::string a, std::string b);
void combineBinaryFiles(const std::string& file1Path, const std::string& file2Path, const std::string& outputPath);
void separateBinaryFile(const std::string& inputPath, const std::string& output1Path, const std::string& output2Path, const char delimiter[], size_t delimiterSize);
void encodeMapToBinaryFile(const std::map<char, std::string>& myMap, const std::string& filename);
std::map<char, std::string> decodeBinaryFileToMap(const std::string& filename);
template <typename T>
void reverseVector(std::vector<T>& vec);
std::map<long, char> reverseMap(const std::map<char, long>& originalMap);
std::map<char, std::string> generateHuffmanCodes(std::vector<long> &A, std::vector<char> &B);
void deflate(std::string filename);
void inflate(std::string filename, std::map<char, std::string> result, std::string opFilename);


#endif