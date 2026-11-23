#ifndef PIPELINE_H
#define PIPELINE_H

/*
 * Glue between the archiver, the compressor and the extractor (step 4 of the plan, variant 1):
 *
 *   folder --archive--> temporary .arc --compress--> <folder>.huff
 *   <folder>.huff --decompress--> temporary .arc --extract--> folder
 *
 * The compressor stays unchanged: it just sees a file whose extension is "arc".
 * A single file is compressed directly, without an archive, exactly as before.
 */

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "compression.h"
#include "decompression.h"
#include "archiver.h"
#include "extractor.h"

using namespace std;
namespace fs = std::filesystem;

struct PipelineResult
{
    string output;           // the .huff file, or the extracted file / folder
    bool is_folder = false;  // the input (compress) or the output (decompress) is a folder
    uint64_t input_bytes = 0;
    uint64_t output_bytes = 0;
    size_t files = 0;        // folders only
    size_t directories = 0;  // folders only
    vector<string> warnings; // skipped links, etc.
};

// total size of all regular files below a folder
inline uint64_t folder_size(const fs::path &folder)
{
    uint64_t total = 0;
    for (const fs::directory_entry &entry : fs::recursive_directory_iterator(folder))
    {
        std::error_code ec;
        if (entry.is_regular_file(ec))
            total += entry.file_size(ec);
    }
    return total;
}

/**
 * @brief Returns a path in `folder` that does not exist yet, for a temporary file:
 *        "<folder>/.<stem>.<random>.tmp.<extension>". It is created next to the final output,
 *        so it is on the same disk (no cross-volume copy) and easy to recognize if left behind.
 */
inline fs::path temporary_path(const fs::path &folder, const string &stem, const string &extension)
{
    static mt19937_64 rng(random_device{}());

    for (;;)
    {
        fs::path candidate = folder / path_from_utf8("." + stem + "." + to_string(rng() % 1000000000ULL) +
                                                     ".tmp." + extension);
        if (!fs::exists(fs::symlink_status(candidate)))
            return candidate;
    }
}

// deletes a temporary file when it goes out of scope (also when an exception is thrown)
struct TemporaryFile
{
    fs::path path;

    explicit TemporaryFile(fs::path p) : path(std::move(p)) {}
    ~TemporaryFile()
    {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;
};

/**
 * @brief Compresses a file or a folder into a .huff file next to it.
 *        A folder "dir/proiect" becomes "dir/proiect.huff" (archived first, then compressed).
 *        Windows symlinks/junctions are not archived; they are reported in `warnings`.
 */
inline PipelineResult compress_path(const string &input)
{
    PipelineResult result;

    fs::path input_path = without_trailing_separator(path_from_utf8(input));
    fs::file_status status = fs::status(input_path);

    if (!fs::exists(status))
        throw runtime_error("Not found: " + input);

    // a single file: compressed directly, as before
    if (!fs::is_directory(status))
    {
        result.output = compress(input);
        result.input_bytes = fs::file_size(input_path);
        result.output_bytes = fs::file_size(path_from_utf8(result.output));
        return result;
    }

    result.is_folder = true;

    string name = utf8_from_path(input_path.filename());
    if (name.empty())
        name = "archive"; // e.g. a whole drive "D:\"

    fs::path parent = input_path.parent_path();
    if (parent.empty() || parent == input_path)
        parent = input_path; // a drive root has no parent: write inside it (the archiver excludes itself)

    fs::path output = parent / path_from_utf8(name + COMPRESSED_EXTENSION);

    // 1. archive into a temporary file next to the output
    TemporaryFile temporary_archive(temporary_path(parent, name, "arc"));

    vector<string> skipped_links;
    archive(utf8_from_path(input_path), utf8_from_path(temporary_archive.path), &skipped_links);

    // 2. compress it; the extension stored in the .huff is "arc", which marks it as a folder
    compress(utf8_from_path(temporary_archive.path), utf8_from_path(output));

    vector<EntryHeader> entries = list_archive(utf8_from_path(temporary_archive.path));
    for (const EntryHeader &entry : entries)
    {
        if (entry.type == EntryType::File)
        {
            result.files++;
            result.input_bytes += entry.size;
        }
        else if (entry.type == EntryType::Dir)
            result.directories++;
    }

    for (const string &link : skipped_links)
        result.warnings.push_back("Link not archived (symlinks/junctions are not supported on Windows): " + link);

    result.output = utf8_from_path(output);
    result.output_bytes = fs::file_size(output);
    return result;
}

/**
 * @brief Decompresses a .huff file. If it contains a folder archive, the folder is extracted
 *        next to the .huff file ("dir/proiect.huff" -> "dir/proiect", or "dir/proiect_decompressed"
 *        if that already exists). Otherwise the original file is restored, as before.
 */
inline PipelineResult decompress_path(const string &compressed_file, const ExtractOptions &options = {})
{
    PipelineResult result;

    fs::path huff_path = path_from_utf8(compressed_file);
    result.input_bytes = fs::file_size(huff_path);

    string extension = read_original_extension(compressed_file);
    if (extension != "arc")
    {
        result.output = decompress(compressed_file);
        result.output_bytes = fs::file_size(path_from_utf8(result.output));
        return result;
    }

    // 1. decompress into a temporary .arc next to the .huff file
    fs::path parent = huff_path.parent_path();
    string stem = utf8_from_path(huff_path.stem());
    TemporaryFile temporary_archive(temporary_path(parent, stem, "arc"));

    decompress(compressed_file, utf8_from_path(temporary_archive.path));

    // a plain .arc file the user compressed (not one of our archives): give it back as a file
    {
        ifstream check(temporary_archive.path, ios::binary);
        char magic[4] = {};
        check.read(magic, 4);
        if (string(magic, 4) != ARCHIVE_MAGIC)
        {
            check.close();
            fs::path restored = path_from_utf8(unique_output_path(utf8_from_path(parent / path_from_utf8(stem)), extension));
            fs::rename(temporary_archive.path, restored);
            result.output = utf8_from_path(restored);
            result.output_bytes = fs::file_size(restored);
            return result;
        }
    }

    // 2. extract into a folder that does not exist yet
    string destination = unique_output_path(utf8_from_path(parent / path_from_utf8(stem)), "");
    ExtractResult extracted;

    try
    {
        extracted = extract(utf8_from_path(temporary_archive.path), destination, options);
    }
    catch (...)
    {
        // the destination was created by us and is incomplete: remove it
        std::error_code ignored;
        fs::remove_all(path_from_utf8(destination), ignored);
        throw;
    }

    result.is_folder = true;
    result.output = destination;
    result.files = extracted.files;
    result.directories = extracted.directories;
    result.output_bytes = folder_size(path_from_utf8(destination));

    for (const string &link : extracted.skipped_symlinks)
        result.warnings.push_back("Symlink not extracted: " + link);

    return result;
}

#endif
