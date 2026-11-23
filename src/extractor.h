#ifndef EXTRACTOR_H
#define EXTRACTOR_H

/*
 * Extractor: reads an (uncompressed) archive written by archiver.h and rebuilds the hierarchy
 * (steps 5-6 of the plan). Every archive is treated as untrusted input:
 *
 *  - paths are normalized and rejected if absolute, if they contain "..", or if they would
 *    end up outside the destination (zip slip)
 *  - symlinks are NOT extracted by default; when allowed, their target is validated too
 *  - lengths are checked before allocating memory, data is copied in 64 KiB chunks
 *  - CRC32 is verified for every entry
 *  - a truncated or corrupted archive gives a clear error and never leaves a partial file
 */

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archiver.h"

using namespace std;
namespace fs = std::filesystem;

// what to do when a file from the archive already exists at the destination
enum class OnExisting
{
    Error,     // stop with an error (default: never overwrite silently)
    Overwrite, // replace the existing file
    Skip       // keep the existing file, skip the entry
};

struct ExtractOptions
{
    OnExisting on_existing = OnExisting::Error;
    bool allow_symlinks = false; // symlinks can be abused (x -> /etc, then x/passwd)
};

struct ExtractResult
{
    size_t files = 0;
    size_t directories = 0;
    size_t symlinks = 0;
    vector<string> skipped_symlinks; // not extracted (allow_symlinks = false)
    vector<string> skipped_existing; // already existed (OnExisting::Skip)
};

// a symlink target longer than this is treated as a corrupted archive
#define MAX_SYMLINK_TARGET 4096

// ---------------------------------------------------------------------------
// Path validation (step 6)
// ---------------------------------------------------------------------------

// removes a trailing separator, so "out/" and "out" compare the same element by element
inline fs::path without_trailing_separator(fs::path p)
{
    if (!p.empty() && p.filename().empty())
        p = p.parent_path();
    return p;
}

// true if `p` is `base` or something below it (element-by-element comparison, both normalized)
inline bool is_within(const fs::path &base, const fs::path &p)
{
    fs::path b = without_trailing_separator(base.lexically_normal());
    fs::path q = without_trailing_separator(p.lexically_normal());

    auto bi = b.begin();
    auto qi = q.begin();
    for (; bi != b.end(); ++bi, ++qi)
    {
        if (qi == q.end() || *qi != *bi)
            return false;
    }
    return true;
}

/**
 * @brief Validates a path read from the archive and returns it normalized and relative.
 *        Accepts "src/main.cpp", "a/./b.txt" (-> "a/b.txt").
 *        Rejects "/etc/passwd", "../secret.txt", "src/../../x", "C:\Windows\x.dll", "", ".".
 * @throws runtime_error if the path is unsafe
 */
inline fs::path validate_archive_path(const string &archive_path)
{
    auto reject = [&](const string &why) {
        throw runtime_error("Unsafe path in archive (" + why + "): " + archive_path);
    };

    if (archive_path.empty())
        reject("empty");
    if (archive_path.find('\0') != string::npos)
        reject("contains a NUL byte");

#ifdef _WIN32
    // the archive always uses '/'; on Windows '\' would be a separator and ':' a drive or a stream
    if (archive_path.find('\\') != string::npos)
        reject("contains '\\'");
    if (archive_path.find(':') != string::npos)
        reject("contains ':'");
#endif

    fs::path p = path_from_utf8(archive_path);

    if (p.is_absolute() || p.has_root_name() || p.has_root_directory())
        reject("absolute path");

    fs::path normal = without_trailing_separator(p.lexically_normal());

    if (normal.empty() || normal == ".")
        reject("refers to the destination itself");

    for (const fs::path &element : normal)
    {
        if (element == "..")
            reject("goes outside the destination");
    }

    return normal;
}

// checks, after resolving the symlinks that already exist on disk, that `target` stays in `dest`
inline void check_real_path_within(const fs::path &dest_real, const fs::path &target, const string &archive_path)
{
    if (!is_within(dest_real, fs::weakly_canonical(target)))
        throw runtime_error("Unsafe path in archive (resolves outside the destination): " + archive_path);
}

// ---------------------------------------------------------------------------
// Reading entries
// ---------------------------------------------------------------------------

inline void read_global_header(istream &in)
{
    char magic[4];
    if (!in.read(magic, 4) || string(magic, 4) != ARCHIVE_MAGIC)
        throw runtime_error("Not a valid archive (wrong magic)");

    uint8_t version = get_u8(in);
    if (version != ARCHIVE_VERSION)
        throw runtime_error("Unsupported archive version: " + to_string(version));

    get_u8(in); // reserved (3 B)
    get_u8(in);
    get_u8(in);
}

// reads one entry header; returns false at END
inline bool read_entry_header(istream &in, EntryHeader &entry)
{
    uint8_t type = get_u8(in);

    if (type == static_cast<uint8_t>(EntryType::End))
        return false;

    if (type != static_cast<uint8_t>(EntryType::File) &&
        type != static_cast<uint8_t>(EntryType::Dir) &&
        type != static_cast<uint8_t>(EntryType::Symlink))
        throw runtime_error("Corrupted archive: unknown entry type " + to_string(type));

    entry.type = static_cast<EntryType>(type);
    entry.path = get_str(in);
    entry.perms = get_u32(in);
    entry.mtime = static_cast<int64_t>(get_u64(in));
    entry.size = get_u64(in);
    entry.link_target.clear();

    // plausibility checks before using size for anything
    if (entry.type == EntryType::Dir && entry.size != 0)
        throw runtime_error("Corrupted archive: directory with data: " + entry.path);
    if (entry.type == EntryType::Symlink && (entry.size == 0 || entry.size > MAX_SYMLINK_TARGET))
        throw runtime_error("Corrupted archive: invalid symlink target length: " + entry.path);

    return true;
}

// copies exactly `size` bytes from `in` to `out` (out may be null: skip), returns their CRC32
inline uint32_t copy_data(istream &in, ostream *out, uint64_t size, const string &entry_path)
{
    vector<char> buffer(64 * 1024);
    uint32_t crc = 0;
    uint64_t remaining = size;

    while (remaining > 0)
    {
        size_t chunk = static_cast<size_t>((std::min<uint64_t>)(remaining, buffer.size()));
        in.read(buffer.data(), static_cast<streamsize>(chunk));

        if (static_cast<size_t>(in.gcount()) != chunk)
            throw runtime_error("Truncated archive: data of " + entry_path + " is incomplete");

        if (out != nullptr)
        {
            out->write(buffer.data(), static_cast<streamsize>(chunk));
            if (!*out)
                throw runtime_error("Error while writing " + entry_path + " (disk full?)");
        }

        crc = crc32_update(crc, buffer.data(), chunk);
        remaining -= chunk;
    }

    return crc;
}

inline void check_crc(istream &in, uint32_t computed, const string &entry_path)
{
    uint32_t stored = get_u32(in);
    if (stored != computed)
        throw runtime_error("Corrupted archive: CRC32 mismatch for " + entry_path);
}

inline void check_end_of_stream(istream &in)
{
    if (in.peek() != char_traits<char>::eof())
        throw runtime_error("Corrupted archive: unexpected data after END");
}

// ---------------------------------------------------------------------------
// list (bonus): reads the headers and skips the data
// ---------------------------------------------------------------------------

/**
 * @brief Lists the entries of an archive without extracting anything.
 *        Data is read only to verify the CRC32, never kept in memory.
 */
inline vector<EntryHeader> list_archive(istream &in)
{
    read_global_header(in);

    vector<EntryHeader> entries;
    EntryHeader entry(EntryType::File, "", 0, 0, 0);

    while (read_entry_header(in, entry))
    {
        uint32_t crc;
        if (entry.type == EntryType::Symlink)
        {
            entry.link_target.assign(static_cast<size_t>(entry.size), '\0');
            if (!in.read(&entry.link_target[0], static_cast<streamsize>(entry.size)))
                throw runtime_error("Truncated archive: data of " + entry.path + " is incomplete");
            crc = crc32_update(0, entry.link_target.data(), entry.link_target.size());
        }
        else
        {
            crc = copy_data(in, nullptr, entry.size, entry.path);
        }

        check_crc(in, crc, entry.path);
        entries.push_back(entry);
    }

    check_end_of_stream(in);
    return entries;
}

inline vector<EntryHeader> list_archive(const string &archive_file)
{
    ifstream in(path_from_utf8(archive_file), ios::binary);
    if (!in)
        throw runtime_error("Can't open: " + archive_file);
    return list_archive(in);
}

// ---------------------------------------------------------------------------
// Extraction (step 5)
// ---------------------------------------------------------------------------

/**
 * @brief Sets the last modification time (Unix seconds) of a file or directory.
 *        On Windows the Win32 API is used, because MinGW's fs::last_write_time can't change
 *        the time of a directory ("Permission denied": it opens it without FILE_FLAG_BACKUP_SEMANTICS).
 * @return false if it could not be set
 */
inline bool set_mtime(const fs::path &p, int64_t unix_seconds)
{
#ifdef _WIN32
    HANDLE h = CreateFileW(p.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    // FILETIME = 100 ns intervals since 1601-01-01; the Unix epoch is 11644473600 s later
    uint64_t ticks = static_cast<uint64_t>(unix_seconds + 11644473600LL) * 10000000ULL;
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);

    BOOL ok = SetFileTime(h, nullptr, nullptr, &ft);
    CloseHandle(h);
    return ok != 0;
#else
    std::error_code ec;
    fs::last_write_time(p, from_unix_seconds(unix_seconds), ec);
    return !ec;
#endif
}

// applies mtime and permissions; errors here are not fatal (e.g. unsupported on this filesystem)
inline void apply_metadata(const fs::path &p, uint32_t mode, int64_t mtime)
{
    set_mtime(p, mtime);

    std::error_code ignored;
    fs::permissions(p, static_cast<fs::perms>(mode & 07777), fs::perm_options::replace, ignored);
}

/**
 * @brief Extracts an archive stream into `dest` (created if missing).
 *        Stops at the first problem with an exception; the file being written at that moment
 *        is deleted, so no partial file is left behind.
 */
inline ExtractResult extract_archive(istream &in, const string &dest, const ExtractOptions &options = {})
{
    ExtractResult result;

    fs::path dest_path = path_from_utf8(dest);
    fs::create_directories(dest_path);
    fs::path dest_real = fs::weakly_canonical(dest_path);

    read_global_header(in);

    struct DirMetadata
    {
        fs::path path;
        uint32_t mode;
        int64_t mtime;
    };
    vector<DirMetadata> directories; // metadata applied at the end, in reverse order

    EntryHeader entry(EntryType::File, "", 0, 0, 0);

    while (read_entry_header(in, entry))
    {
        fs::path relative = validate_archive_path(entry.path);
        fs::path target = dest_real / relative;

        // lexical check, then the real one (an existing symlink in dest could point outside)
        if (!is_within(dest_real, target))
            throw runtime_error("Unsafe path in archive (goes outside the destination): " + entry.path);
        check_real_path_within(dest_real, target.parent_path(), entry.path);

        if (entry.type == EntryType::Dir)
        {
            check_crc(in, 0, entry.path);
            fs::create_directories(target);
            check_real_path_within(dest_real, target, entry.path);
            directories.push_back({target, entry.perms, entry.mtime});
            result.directories++;
        }
        else if (entry.type == EntryType::File)
        {
            fs::file_status existing = fs::symlink_status(target);

            if (fs::exists(existing))
            {
                if (fs::is_directory(existing))
                    throw runtime_error("A directory exists where a file should be extracted: " + utf8_from_path(target));

                if (options.on_existing == OnExisting::Error)
                    throw runtime_error("File already exists at the destination: " + utf8_from_path(target));

                if (options.on_existing == OnExisting::Skip)
                {
                    // the data still has to be read (and verified) to reach the next entry
                    check_crc(in, copy_data(in, nullptr, entry.size, entry.path), entry.path);
                    result.skipped_existing.push_back(entry.path);
                    continue;
                }

                // Overwrite: remove first, so we never write through an existing symlink
                // (a read-only file can't be deleted on Windows, so make it writable first)
                std::error_code ignored;
                if (!fs::is_symlink(existing))
                    fs::permissions(target, fs::perms::owner_write, fs::perm_options::add, ignored);
                fs::remove(target);
            }

            fs::create_directories(target.parent_path());

            ofstream out(target, ios::binary | ios::trunc);
            if (!out)
                throw runtime_error("Can't create: " + utf8_from_path(target));

            try
            {
                uint32_t crc = copy_data(in, &out, entry.size, entry.path);
                check_crc(in, crc, entry.path);
                out.close();
                if (!out)
                    throw runtime_error("Error while writing " + entry.path + " (disk full?)");
            }
            catch (...)
            {
                out.close();
                std::error_code ignored;
                fs::remove(target, ignored); // never leave a partial or corrupted file behind
                throw;
            }

            apply_metadata(target, entry.perms, entry.mtime);
            result.files++;
        }
        else // Symlink
        {
            string link_target(static_cast<size_t>(entry.size), '\0');
            if (!in.read(&link_target[0], static_cast<streamsize>(entry.size)))
                throw runtime_error("Truncated archive: data of " + entry.path + " is incomplete");
            check_crc(in, crc32_update(0, link_target.data(), link_target.size()), entry.path);

            if (!options.allow_symlinks)
            {
                result.skipped_symlinks.push_back(entry.path + " -> " + link_target);
                continue;
            }

            // the target must be relative and must stay inside the destination
            fs::path link_path = path_from_utf8(link_target);
            if (link_path.is_absolute() || link_path.has_root_name() || link_path.has_root_directory() ||
                !is_within(dest_real, target.parent_path() / link_path))
                throw runtime_error("Unsafe symlink in archive (target outside the destination): " +
                                    entry.path + " -> " + link_target);

            if (fs::exists(fs::symlink_status(target)))
            {
                if (options.on_existing == OnExisting::Error)
                    throw runtime_error("File already exists at the destination: " + utf8_from_path(target));
                if (options.on_existing == OnExisting::Skip)
                {
                    result.skipped_existing.push_back(entry.path);
                    continue;
                }
                fs::remove(target);
            }

            fs::create_directories(target.parent_path());
            // note: not implemented by MinGW's std::filesystem, and needs admin/Developer Mode on Windows
            fs::create_symlink(link_path, target);
            result.symlinks++;
        }
    }

    check_end_of_stream(in);

    // directories last, deepest first: a read-only directory would block writing its files,
    // and writing a file changes the mtime of its directory
    for (auto it = directories.rbegin(); it != directories.rend(); ++it)
        apply_metadata(it->path, it->mode, it->mtime);

    return result;
}

/**
 * @brief Extracts an archive file into the folder `dest`.
 */
inline ExtractResult extract(const string &archive_file, const string &dest, const ExtractOptions &options = {})
{
    ifstream in(path_from_utf8(archive_file), ios::binary);
    if (!in)
        throw runtime_error("Can't open: " + archive_file);
    return extract_archive(in, dest, options);
}

#endif
