#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "classes.h"
#include "huffman.h"

using namespace std;

/*
 * .huff file format (all integers little-endian):
 *
 *   4 bytes   magic "HUF1"
 *   uint32    length of the original extension (without the dot)
 *   N bytes   original extension, e.g. "zip" (empty if the file had none)
 *   uint16    number of distinct bytes (0..256)
 *   repeated: uint8 symbol, uint64 frequency
 *   rest      Huffman-encoded payload, MSB first, last byte zero-padded
 */
#define HUFF_MAGIC "HUF1"

// ---------- UTF-8 path helpers ----------
// file names are kept as UTF-8 std::string (what Qt's toStdString() returns);
// these convert them to/from filesystem::path so non-ASCII names work on Windows.
// filesystem::u8path is deprecated since C++20, so it is only used before C++20.

inline filesystem::path path_from_utf8(const string &s)
{
#if defined(__cpp_char8_t)
    return filesystem::path(std::u8string(s.begin(), s.end()));
#else
    return filesystem::u8path(s);
#endif
}

inline string utf8_from_path(const filesystem::path &p)
{
    auto u = p.u8string(); // std::string before C++20, std::u8string since C++20
    return string(u.begin(), u.end());
}

// ---------- little-endian helpers (format independent of platform) ----------

inline void write_uint(ofstream &file, uint64_t value, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++)
    {
        char b = static_cast<char>((value >> (8 * i)) & 0xFF);
        file.write(&b, 1);
    }
}

inline uint64_t read_uint(ifstream &file, int num_bytes)
{
    uint64_t value = 0;
    for (int i = 0; i < num_bytes; i++)
    {
        char b;
        if (!file.read(&b, 1))
            throw runtime_error("Corrupted .huff file: unexpected end of header");
        value |= static_cast<uint64_t>(static_cast<unsigned char>(b)) << (8 * i);
    }
    return value;
}

// ---------- file I/O ----------

// reads the whole file as raw bytes
inline string read_file_to_string(const string &filename)
{
    ifstream inFile(path_from_utf8(filename), ios::binary);

    if (!inFile)
        throw runtime_error("Can't open: " + filename);

    ostringstream contents;
    contents << inFile.rdbuf();
    return contents.str();
}

// writes raw bytes (binary mode: no \n -> \r\n translation on Windows)
inline void write_string_to_file(const string &filename, const string &data)
{
    ofstream file(path_from_utf8(filename), ios::binary);

    if (!file)
        throw runtime_error("Can't create: " + filename);

    file.write(data.data(), static_cast<streamsize>(data.size()));
}

inline void write_metadata_to_binary_file(ofstream &file, const string &original_extension, const map<unsigned char, uint64_t> &freq_dict)
{
    file.write(HUFF_MAGIC, 4);

    write_uint(file, original_extension.size(), 4);
    file.write(original_extension.data(), static_cast<streamsize>(original_extension.size()));

    write_uint(file, freq_dict.size(), 2);

    for (const auto &p : freq_dict)
    {
        write_uint(file, p.first, 1);
        write_uint(file, p.second, 8);
    }
}

inline void write_byte_stream_to_binary_file(ofstream &file, const vector<unsigned char> &byte_stream)
{
    file.write(reinterpret_cast<const char *>(byte_stream.data()), static_cast<streamsize>(byte_stream.size()));
}

inline void read_binary_file(ifstream &input_file, string &original_extension, map<unsigned char, uint64_t> &freq_dict, vector<unsigned char> &byte_stream)
{
    char magic[4];
    if (!input_file.read(magic, 4) || string(magic, 4) != HUFF_MAGIC)
        throw runtime_error("Not a valid .huff file");

    uint64_t ext_size = read_uint(input_file, 4);
    if (ext_size > 4096)
        throw runtime_error("Corrupted .huff file: invalid extension length");

    original_extension.assign(static_cast<size_t>(ext_size), '\0');
    if (ext_size > 0 && !input_file.read(&original_extension[0], static_cast<streamsize>(ext_size)))
        throw runtime_error("Corrupted .huff file: unexpected end of header");

    uint64_t freq_size = read_uint(input_file, 2);
    if (freq_size > 256)
        throw runtime_error("Corrupted .huff file: invalid symbol count");

    for (uint64_t i = 0; i < freq_size; ++i)
    {
        unsigned char symbol = static_cast<unsigned char>(read_uint(input_file, 1));
        freq_dict[symbol] = read_uint(input_file, 8);
    }

    // the rest of the file is the payload
    byte_stream.assign(istreambuf_iterator<char>(input_file), istreambuf_iterator<char>());
}

// ---------- paths ----------

/**
 * @brief Separates the file name and the extension
 * @param file The file with its extension (may contain directories)
 * @return Returns a pair: first element is the path without the last extension
 *         (e.g. "C:/dir.x/archive.tar" for "C:/dir.x/archive.tar.gz"),
 *         second element is the last extension without the dot ("gz"), or "" if there is none
 */
inline pair<string, string> separate_extension(const string &file)
{
    filesystem::path p = path_from_utf8(file);

    string extension = utf8_from_path(p.extension());
    if (!extension.empty() && extension[0] == '.')
        extension.erase(0, 1);

    filesystem::path without_extension = p;
    without_extension.replace_extension();

    return {utf8_from_path(without_extension), extension};
}

/**
 * @brief Returns a path that does not exist yet, so an existing file is never overwritten.
 *        "dir/name.ext" -> "dir/name.ext", or "dir/name_decompressed.ext", "dir/name_decompressed_2.ext", ...
 */
inline string unique_output_path(const string &name_without_extension, const string &extension)
{
    string dot_ext = extension.empty() ? "" : "." + extension;

    string candidate = name_without_extension + dot_ext;
    if (!filesystem::exists(path_from_utf8(candidate)))
        return candidate;

    candidate = name_without_extension + "_decompressed" + dot_ext;
    for (int i = 2; filesystem::exists(path_from_utf8(candidate)); i++)
        candidate = name_without_extension + "_decompressed_" + to_string(i) + dot_ext;

    return candidate;
}

#endif
