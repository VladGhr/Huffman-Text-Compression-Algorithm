# Huffman Compressor

A C++17 compressor based on static Huffman coding, with a Qt 6 desktop interface. You can compress any single file or a whole folder into a `.huff` file and restore it later, byte for byte.

Folders go through a small archive format of its own (VARC, similar to tar) before compression. Every file in the archive carries a CRC32 checksum, so you get an error instead of silently broken output if the archive is damaged.

## What you can do with it

- Compress any file: text, executables, images, archives. The compressor works on raw bytes (`unsigned char`), so binary files survive the round trip unchanged.
- Compress a folder with all its subfolders, empty folders included. `proiect/` becomes `proiect.huff`.
- Decompress a `.huff` file. A single file gets its original extension back. A folder is extracted next to the `.huff` file.
- Drag a file or folder onto the window, or pick one with **Browse file...** / **Browse folder...**.

Nothing gets overwritten. If `report.pdf` or the folder `proiect/` already exists, the output goes to `report_decompressed.pdf` or `proiect_decompressed/`.

## Results

Measured on Windows 10 with the current version:

| Input | Before | After | Size after |
|---|---|---|---|
| Folder with 3 source/text files | 108,913 B | 53,985 B | 49.6% |
| Data where 80% of bytes are the same value | 200,000 B | 30,050 B | 15.0% |
| `libstdc++-6.dll` | 2.16 MiB | 1.80 MiB | 83.3% |
| Folder with 12 files (C++ sources and one DLL) | 2.24 MiB | 1.87 MiB | 83.5% |
| Random bytes | 300,000 B | 302,317 B | 100.8% |

Text and repetitive data compress well. Random data and files that are already compressed (`.zip`, `.jpg`, `.mp4`, `.7z`) do not. Their bytes are close to uniformly distributed, so Huffman coding has nothing to gain, and the header adds up to 2.3 KB. Expect such files to grow slightly.

## Building

You need:

- a C++17 compiler (tested with GCC 13.1 from MSYS2 UCRT64)
- CMake 3.16 or newer
- Qt 6 (Widgets module)

The project has only been built and tested on Windows 10. The archive and extraction code has Windows-specific parts behind `#ifdef _WIN32` and uses portable `std::filesystem` code elsewhere, but nobody has compiled it on Linux or macOS yet.

### Windows with MSYS2

Install the toolchain from the **MSYS2 UCRT64** shell:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base
```

Configure and build:

```sh
cmake -S . -B out -G Ninja
cmake --build out
```

Build into a separate folder such as `out`. The `build/` folder in the repository holds generated files from the author's machine, with absolute paths like `E:/Huffman-Text-Compression-Algorithm` inside `build.ninja`, so you are better off starting clean.

### Running

The executable needs the Qt DLLs. Either run it from the UCRT64 shell, where they are on `PATH`:

```sh
./out/compressor.exe
```

or copy the DLLs next to the executable once, so it also starts with a double click:

```sh
windeployqt6 out/compressor.exe
```

The app is built as a Windows GUI program, so no console window opens behind it. If you want `std::cout` output while debugging, remove the `WIN32_EXECUTABLE` line from `CMakeLists.txt`.

## Using the code without the interface

All the logic lives in header-only files under `src/`. Include `pipeline.h` and call two functions:

```cpp
#include "pipeline.h"

int main()
{
    try
    {
        PipelineResult c = compress_path("C:/work/proiect");      // folder or file
        // c.output is the .huff file next to the folder: C:/work/proiect.huff
        // c.files, c.directories, c.input_bytes and c.output_bytes describe the result

        PipelineResult d = decompress_path("C:/work/proiect.huff");
        // d.output is the extracted folder: C:/work/proiect_decompressed while proiect/ still exists
        for (const std::string &warning : d.warnings)
            std::cout << warning << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";   // missing input, corrupted file, unsafe path, ...
    }
}
```

Paths are UTF-8 `std::string`s, so names with diacritics work on Windows too. Every function reports problems by throwing `std::runtime_error` (or `std::filesystem::filesystem_error`) with a readable message. Wrap your calls in `try`/`catch`. If you call them from a worker thread, an uncaught exception terminates the whole program.

The lower layers are usable on their own:

| Function | Header | What it does |
|---|---|---|
| `compress(file, output = "")` | `compression.h` | Compresses one file into `.huff` |
| `decompress(file, output = "")` | `decompression.h` | Restores one file |
| `archive(folder, file.arc)` | `archiver.h` | Writes an uncompressed VARC archive |
| `extract(file.arc, folder, options)` | `extractor.h` | Extracts a VARC archive |
| `list_archive(file.arc)` | `extractor.h` | Lists entries and checks every CRC32 without writing anything |

## File formats

All integers are little-endian and written byte by byte. The code never writes a struct directly with `fwrite`, because padding and byte order would then depend on the compiler and the machine.

### `.huff`

| Field | Size | Content |
|---|---|---|
| magic | 4 B | `HUF1` |
| extension length | 4 B | length of the original extension |
| extension | N B | for example `pdf`, or `arc` for a compressed folder |
| symbol count | 2 B | distinct byte values in the input, 0 to 256 |
| table | 9 B per symbol | byte value (1 B) and its frequency (8 B) |
| payload | rest | Huffman codes, most significant bit first, last byte padded with zeros |

The decoder rebuilds the tree from the frequency table. To get the same tree on both sides, nodes with equal frequency are ordered by a fixed id (the byte value for leaves, 256 and up for internal nodes). Without that rule the result would depend on how the standard library's heap breaks ties.

### VARC archive

An 8-byte header (`VARC`, version `1`, 3 reserved bytes), then one entry per file, folder or symlink, then a single `0xFF` byte as the END marker.

| Field | Size | Content |
|---|---|---|
| type | 1 B | 0 file, 1 folder, 2 symlink |
| path length | 2 B | |
| path | N B | relative, UTF-8, `/` as separator on every platform |
| mode | 4 B | permissions, for example `0644` |
| mtime | 8 B | last modification, signed seconds since 1970 |
| size | 8 B | bytes of data, 0 for folders |
| data | size B | file content, or the symlink target |
| crc32 | 4 B | CRC32 of the data |

Entries are stored in preorder and sorted by name inside each folder. A folder always comes before its contents, so it exists by the time the extractor writes files into it. The same folder always produces the same archive bytes, which makes tests easy to write: archive twice and compare.

A compressed folder is simply a VARC archive passed through the Huffman compressor, the same split as `tar` + `gzip`. The compressor sees one stream, so it also benefits from repetition between files. The downside: to get one file out, you have to decompress everything.

## Safety checks during extraction

The extractor treats every archive as untrusted input. A crafted archive with an entry like `../../.bashrc` could otherwise write outside the target folder (the "zip slip" attack).

| Path in the archive | Result |
|---|---|
| `src/main.cpp` | extracted |
| `a/./b.txt` | extracted as `a/b.txt` |
| `/etc/passwd` | rejected, absolute path |
| `C:\Windows\x.dll` | rejected, absolute path |
| `../secret.txt` | rejected, leaves the target folder |
| `src/../../x` | rejected, leaves the target folder after normalization |

Other checks:

- Each file's CRC32 is verified. If it fails, or the archive ends early, the half-written file is deleted and you get an error.
- Lengths read from the archive are checked before any memory is allocated based on them.
- Symlinks are not extracted unless you set `ExtractOptions::allow_symlinks`. Even then, a link whose target leaves the folder is rejected. Otherwise an archive could create `x -> /etc` and then write `x/passwd`.
- Existing files are never overwritten unless you ask for it with `OnExisting::Overwrite` or `OnExisting::Skip`. The default is to stop with an error.
- Modification times and read-only flags are restored. Folder times are set last, deepest folder first, because writing a file changes its folder's timestamp.

## Limitations

- **The whole input is loaded into memory.** Compressing a 4 GB folder needs more than 4 GB of RAM: the archive is read whole, and the compressed output is built in memory before it is written. Static Huffman needs all byte frequencies before it can encode anything, so streaming would require a second pass over the file on disk or compression in independent blocks.
- **The `.huff` header costs 9 bytes per distinct byte value**, up to 2.3 KB. A canonical Huffman code would only need the code lengths, 256 bytes at most.
- **Symlinks and junctions are skipped on Windows.** The MinGW version of `std::filesystem` does not detect them and walks into them (a junction pointing to its parent folder loops forever), and it does not implement `read_symlink`. The archiver detects them through the Win32 API instead, skips them and lists them as warnings in the interface. Symlink support on Linux is implemented but untested.
- **No automated tests in the repository yet.** The round trips, corrupted archives and unsafe paths described above were tested with standalone programs during development, but those programs are not part of the repo.

## Project layout

```
main.cpp            Qt interface
CMakeLists.txt
src/
  classes.h         Huffman tree, node comparator, archive entry types
  huffman.h         frequency table, code building, bit-level encoding and decoding
  utils.h           .huff format, binary I/O, UTF-8 path helpers
  compression.h     compress one file
  decompression.h   decompress one file
  archiver.h        folder to VARC archive, CRC32
  extractor.h       VARC archive to folder, path validation
  pipeline.h        folder or file to .huff and back
  debug.h           helpers that print the tree, heap and code tables
```
