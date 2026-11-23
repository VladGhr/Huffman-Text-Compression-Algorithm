#ifndef ARCHIVER_H
#define ARCHIVER_H

/*
 * Archiver: turns a file hierarchy into a single byte stream (steps 1-3 of the plan).
 * It knows nothing about compression; the result can be given to compress() afterwards.
 *
 * Archive format (all numbers little-endian):
 *
 *   global header (8 B):  magic "VARC" (4 B) | version (1 B) | reserved (3 B, zero)
 *   entries, one after another:
 *       type      1 B   0 File, 1 Dir, 2 Symlink
 *       path_len  2 B   length of path in bytes
 *       path      N B   relative path, UTF-8, '/' separator, no trailing '\0'
 *       mode      4 B   permissions (e.g. 0644)
 *       mtime     8 B   last modification, seconds since the Unix epoch (signed)
 *       size      8 B   bytes of data (0 for Dir)
 *       data   size B   file content, or the symlink target
 *       crc32     4 B   CRC32 over data
 *   END entry:          type = 0xFF only
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "classes.h"
#include "utils.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace std;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Windows links.
// MinGW's std::filesystem does not recognize Windows symlinks and junctions: is_symlink() is
// false and the iterator walks into them (a junction to a parent folder loops forever), and
// read_symlink() is not implemented. So on Windows they are detected with the Win32 API.
// Only real links count: other reparse points (e.g. OneDrive "Files On-Demand") are normal files.
// ---------------------------------------------------------------------------

inline bool is_windows_link(const fs::path &p)
{
#ifdef _WIN32
    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(p.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    FindClose(h);

    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;

    // for reparse points, dwReserved0 holds the reparse tag
    return data.dwReserved0 == IO_REPARSE_TAG_SYMLINK || data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT;
#else
    (void)p;
    return false;
#endif
}

#define ARCHIVE_MAGIC "VARC"
#define ARCHIVE_VERSION 1
#define ARCHIVE_EXTENSION ".arc"

// ---------------------------------------------------------------------------
// put_* / get_* : write and read numbers byte by byte, little-endian.
// Never write a struct directly (padding and endianness would end up in the file).
// ---------------------------------------------------------------------------

inline void put_u8(ostream &out, uint8_t value)
{
    out.put(static_cast<char>(value));
}

inline void put_u16(ostream &out, uint16_t value)
{
    put_u8(out, static_cast<uint8_t>(value & 0xFF));
    put_u8(out, static_cast<uint8_t>(value >> 8));
}

inline void put_u32(ostream &out, uint32_t value)
{
    put_u16(out, static_cast<uint16_t>(value & 0xFFFF));
    put_u16(out, static_cast<uint16_t>(value >> 16));
}

inline void put_u64(ostream &out, uint64_t value)
{
    put_u32(out, static_cast<uint32_t>(value & 0xFFFFFFFF));
    put_u32(out, static_cast<uint32_t>(value >> 32));
}

// u16 length + bytes
inline void put_str(ostream &out, const string &s)
{
    if (s.size() > 0xFFFF)
        throw runtime_error("Path too long for the archive (over 65535 bytes): " + s.substr(0, 100) + "...");

    put_u16(out, static_cast<uint16_t>(s.size()));
    out.write(s.data(), static_cast<streamsize>(s.size()));
}

inline uint8_t get_u8(istream &in)
{
    char c;
    if (!in.get(c))
        throw runtime_error("Truncated archive: unexpected end of data");
    return static_cast<uint8_t>(c);
}

inline uint16_t get_u16(istream &in)
{
    uint16_t low = get_u8(in);
    uint16_t high = get_u8(in);
    return static_cast<uint16_t>(low | (high << 8));
}

inline uint32_t get_u32(istream &in)
{
    uint32_t low = get_u16(in);
    uint32_t high = get_u16(in);
    return low | (high << 16);
}

inline uint64_t get_u64(istream &in)
{
    uint64_t low = get_u32(in);
    uint64_t high = get_u32(in);
    return low | (high << 32);
}

inline string get_str(istream &in)
{
    uint16_t len = get_u16(in);
    string s(len, '\0');
    if (len > 0 && !in.read(&s[0], len))
        throw runtime_error("Truncated archive: unexpected end of data");
    return s;
}

// ---------------------------------------------------------------------------
// CRC32 (the standard one used by zip/gzip/png, polynomial 0xEDB88320).
// Computed incrementally: crc = crc32_update(crc, chunk) for every chunk, starting from 0.
// ---------------------------------------------------------------------------

inline const array<uint32_t, 256> &crc32_table()
{
    static const array<uint32_t, 256> table = []() {
        array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();
    return table;
}

inline uint32_t crc32_update(uint32_t crc, const char *data, size_t length)
{
    const array<uint32_t, 256> &table = crc32_table();
    crc = ~crc;
    for (size_t i = 0; i < length; i++)
        crc = table[(crc ^ static_cast<unsigned char>(data[i])) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// ---------------------------------------------------------------------------
// Metadata helpers
// ---------------------------------------------------------------------------

// file_time_type -> seconds since the Unix epoch.
// C++17 has no clock_cast, so the (constant, whole-second) offset between the filesystem
// clock and the system clock is measured once and rounded, which keeps the result exact
// and identical between runs.
inline std::chrono::seconds file_clock_to_unix_offset()
{
    using namespace std::chrono;

    static const seconds offset = []() {
        auto file_now = duration_cast<nanoseconds>(fs::file_time_type::clock::now().time_since_epoch());
        auto sys_now = duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
        return floor<seconds>(sys_now - file_now + milliseconds(500)); // round to nearest second
    }();

    return offset;
}

inline int64_t to_unix_seconds(fs::file_time_type file_time)
{
    auto since_file_epoch = std::chrono::floor<std::chrono::seconds>(file_time.time_since_epoch());
    return (since_file_epoch + file_clock_to_unix_offset()).count();
}

// inverse of to_unix_seconds (used by the extractor to restore mtime)
inline fs::file_time_type from_unix_seconds(int64_t unix_seconds)
{
    auto since_file_epoch = std::chrono::seconds(unix_seconds) - file_clock_to_unix_offset();
    return fs::file_time_type(std::chrono::duration_cast<fs::file_time_type::duration>(since_file_epoch));
}

inline uint32_t to_mode(fs::perms p)
{
    return static_cast<uint32_t>(p) & 07777;
}

// path -> UTF-8 string with '/' as separator (also on Windows)
inline string to_archive_path(const fs::path &p)
{
    auto u = p.generic_u8string(); // std::string before C++20, std::u8string since C++20
    return string(u.begin(), u.end());
}

// ---------------------------------------------------------------------------
// Step 2: collect the entries
// ---------------------------------------------------------------------------

/**
 * @brief Walks `root` recursively and returns one EntryHeader per file, directory and symlink.
 *
 * - Preorder: a directory always comes before its children.
 * - Sorted alphabetically inside each directory, so the same hierarchy always gives
 *   the same archive, byte for byte.
 * - Paths are relative to root, UTF-8, with '/' as separator. The root itself is not included.
 * - If root is a single file, the result contains only that file (path = its name).
 * - Symlinks are recorded as links (target saved), never followed.
 *   On Windows (MinGW) symlinks and junctions can't be read: they are not followed, not archived,
 *   and their paths are added to `skipped_links` (if given) so the caller can warn the user.
 * - Special files (socket, FIFO, device) are reported as errors.
 *
 * @param root          Folder (or single file) to archive, UTF-8
 * @param exclude       Optional file to leave out (the archive being written, if it is inside root)
 * @param skipped_links Optional: receives the relative paths of the Windows links that were skipped
 * @throws runtime_error / filesystem_error on unreadable entries or unsupported file types
 */
inline vector<EntryHeader> create_entries(const string &root, const string &exclude = "",
                                          vector<string> *skipped_links = nullptr)
{
    vector<EntryHeader> entries;

    fs::path root_path = path_from_utf8(root);
    fs::file_status root_status = fs::symlink_status(root_path);

    if (!fs::exists(root_status))
        throw runtime_error("Not found: " + root);

    fs::path excluded;
    if (!exclude.empty())
        excluded = fs::weakly_canonical(path_from_utf8(exclude));

    // builds the header for one path; `relative` is what gets stored in the archive
    auto make_entry = [&](const fs::path &p, const fs::file_status &status, const fs::path &relative) {
        string rel = to_archive_path(relative);

        if (fs::is_symlink(status))
        {
            // data = the link target. Permissions don't matter for a link, and
            // last_write_time() would follow the link (and throw if it is broken), so mtime = 0.
            string target = to_archive_path(fs::read_symlink(p));
            entries.emplace_back(EntryType::Symlink, rel, 0777, 0, target.size(), target);
        }
        else if (fs::is_directory(status))
        {
            entries.emplace_back(EntryType::Dir, rel, to_mode(status.permissions()),
                                 to_unix_seconds(fs::last_write_time(p)), 0);
        }
        else if (fs::is_regular_file(status))
        {
            entries.emplace_back(EntryType::File, rel, to_mode(status.permissions()),
                                 to_unix_seconds(fs::last_write_time(p)), fs::file_size(p));
        }
        else
        {
            throw runtime_error("Unsupported file type (socket, FIFO or device): " + utf8_from_path(p));
        }
    };

    // a single file (or link) given as argument: the archive contains only that entry
    if (!fs::is_directory(root_status))
    {
        make_entry(root_path, root_status, root_path.filename());
        return entries;
    }

    // symlinks to directories are not followed (no directory_options::follow_directory_symlink),
    // so there can be no infinite loops
    for (fs::recursive_directory_iterator it(root_path), end; it != end; ++it)
    {
        const fs::directory_entry &entry = *it;
        const fs::path &p = entry.path();

        // canonicalize only the parent, so a symlink is not resolved to its target
        if (!excluded.empty() && fs::weakly_canonical(p.parent_path()) / p.filename() == excluded)
            continue;

        if (is_windows_link(p))
        {
            it.disable_recursion_pending(); // never walk into it
            if (skipped_links != nullptr)
                skipped_links->push_back(to_archive_path(p.lexically_relative(root_path)));
            continue;
        }

        // symlink_status, not status: a symlink must be recorded as a link, not followed
        make_entry(p, entry.symlink_status(), p.lexically_relative(root_path));
    }

    // fs::path compares element by element ("a/b" vs "a-b" compares "a" first), which gives
    // exactly preorder + alphabetical order inside every directory
    sort(entries.begin(), entries.end(), [](const EntryHeader &a, const EntryHeader &b) {
        return path_from_utf8(a.path) < path_from_utf8(b.path);
    });

    return entries;
}

// ---------------------------------------------------------------------------
// Step 3: serialization
// ---------------------------------------------------------------------------

inline void write_global_header(ostream &out)
{
    out.write(ARCHIVE_MAGIC, 4);
    put_u8(out, ARCHIVE_VERSION);
    put_u8(out, 0); // reserved (3 B)
    put_u8(out, 0);
    put_u8(out, 0);
}

inline void write_entry_header(ostream &out, const EntryHeader &entry)
{
    put_u8(out, static_cast<uint8_t>(entry.type));
    put_str(out, entry.path);
    put_u32(out, entry.perms);
    put_u64(out, static_cast<uint64_t>(entry.mtime));
    put_u64(out, entry.size);
}

/**
 * @brief Writes one entry: header, data, CRC32.
 *        File content is copied in 64 KiB chunks (never fully in RAM) and the CRC is computed
 *        while copying, so the file is read only once.
 * @param base Folder the entry paths are relative to (for a single file: its parent folder)
 */
inline void write_entry(ostream &out, const fs::path &base, const EntryHeader &entry)
{
    write_entry_header(out, entry);

    uint32_t crc = 0;

    if (entry.type == EntryType::File)
    {
        fs::path source = base / path_from_utf8(entry.path);
        ifstream in(source, ios::binary);
        if (!in)
            throw runtime_error("Can't open: " + utf8_from_path(source));

        vector<char> buffer(64 * 1024);
        uint64_t remaining = entry.size;

        while (remaining > 0)
        {
            size_t chunk = static_cast<size_t>((std::min<uint64_t>)(remaining, buffer.size()));
            in.read(buffer.data(), static_cast<streamsize>(chunk));
            size_t got = static_cast<size_t>(in.gcount());

            if (got != chunk)
                throw runtime_error("File got shorter while archiving: " + entry.path);

            out.write(buffer.data(), static_cast<streamsize>(got));
            crc = crc32_update(crc, buffer.data(), got);
            remaining -= got;
        }

        // exactly `size` bytes must have been copied: if more are left, the file changed
        if (in.peek() != char_traits<char>::eof())
            throw runtime_error("File got larger while archiving: " + entry.path);
    }
    else if (entry.type == EntryType::Symlink)
    {
        out.write(entry.link_target.data(), static_cast<streamsize>(entry.link_target.size()));
        crc = crc32_update(crc, entry.link_target.data(), entry.link_target.size());
    }
    // Dir: no data (size = 0), CRC of nothing = 0

    put_u32(out, crc);

    if (!out)
        throw runtime_error("Error while writing the archive (disk full?)");
}

/**
 * @brief Writes a complete archive (global header, all entries, END) to `out`.
 * @param root    The folder (or single file) the entries were collected from
 * @param entries Result of create_entries(root)
 */
inline void write_archive(ostream &out, const string &root, const vector<EntryHeader> &entries)
{
    fs::path root_path = path_from_utf8(root);

    // for a single file the entry path is its name, so data is read relative to its folder
    fs::path base = fs::is_directory(fs::symlink_status(root_path)) ? root_path : root_path.parent_path();

    write_global_header(out);

    for (const EntryHeader &entry : entries)
        write_entry(out, base, entry);

    put_u8(out, static_cast<uint8_t>(EntryType::End));

    if (!out)
        throw runtime_error("Error while writing the archive (disk full?)");
}

/**
 * @brief Archives a folder (or a single file) into `archive_file` (uncompressed).
 *        If the archive is created inside the source folder, it is excluded from itself.
 *        On error the partial archive is deleted and the exception is rethrown.
 * @param skipped_links Optional: receives the Windows links that were skipped (see create_entries)
 * @return The path of the archive
 */
inline string archive(const string &source, const string &archive_file,
                      vector<string> *skipped_links = nullptr)
{
    vector<EntryHeader> entries = create_entries(source, archive_file, skipped_links);

    fs::path archive_path = path_from_utf8(archive_file);
    ofstream out(archive_path, ios::binary);
    if (!out)
        throw runtime_error("Can't create: " + archive_file);

    try
    {
        write_archive(out, source, entries);
        out.close();
        if (!out)
            throw runtime_error("Error while writing: " + archive_file);
    }
    catch (...)
    {
        out.close();
        std::error_code ignored;
        fs::remove(archive_path, ignored); // never leave a partial archive behind
        throw;
    }

    return archive_file;
}

#endif
