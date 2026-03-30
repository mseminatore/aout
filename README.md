# aout library

[![CMake](https://github.com/mseminatore/aout/actions/workflows/cmake.yml/badge.svg)](https://github.com/mseminatore/aout/actions/workflows/cmake.yml)

A C library for reading, writing, and manipulating Unix a.out object files, along
with a suite of command-line binary utility tools.

## Features

- Read and write a.out format object files
- Manipulate text, data, and BSS segments
- Symbol table management with fast O(1) hash-based lookup
- Relocation entry handling (absolute and PC-relative, 1/2/4/8-byte patches)
- Object file concatenation and linking
- Archive (`.a`) file creation, listing, extraction, and member deletion
- Debug output and hex dumps
- CLI tools: `dump`, `strip`, `ld`, `ar`

## Building

### Using Make

```bash
make
make test
```

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
ctest
```

## CLI Tools

### `dump` — display object file contents

```
Usage: dump [options] filename

  -h    dump header (default)
  -t    dump text segment (hex)
  -d    dump data segment (hex)
  -r    dump relocation entries
  -s    dump symbol table
  -a    dump all sections
```

### `strip` — remove symbols and/or relocations

```
Usage: strip [options] input [output]

  -r         also strip relocation entries
  -a         strip everything (symbols + relocations)
  -o file    write output to file (default: a.out)
```

With no flags, `strip` removes the symbol table only (standard Unix behavior).

### `ld` — link object files

```
Usage: ld [options] file1.o [file2.o ...]

  -o outfile    output filename (default: a.out)
  -b addr       base address for text segment (default: 0)
```

Links object files left-to-right using `aout_concat` and `aout_relocate`. Handles
absolute and PC-relative relocations for any patch size encoded in the relocation entry.

### `ar` — create and manage static libraries

```
Usage: ar {r|t|x|d}[v] archive [file ...]

  r    add or replace members in archive
  t    list members in archive
  x    extract members from archive
  d    delete members from archive
  v    verbose output (combine with any operation)
```

Examples:

```bash
ar rv libfoo.a foo.o bar.o   # create library (verbose)
ar t  libfoo.a               # list members
ar tv libfoo.a               # list with mode, size, date
ar x  libfoo.a               # extract all members
ar x  libfoo.a foo.o         # extract one member
ar d  libfoo.a bar.o         # delete a member
```

Uses the standard System V ar format, compatible with `ar(1)` and linkers that
consume `.a` static libraries.

## C API Example

```c
#include "aout.h"

int main(void)
{
    // Create a new object file
    aout_object_file_t *obj = aout_create();

    // Add code to text segment
    aout_add_text(obj, 0x90);  // NOP instruction
    aout_add_text(obj, 0xA9);  // LDA immediate
    aout_add_text(obj, 0x42);  // value

    // Add a public symbol at the start of text
    aout_symbol_t sym = { .type = AOUT_SET_TEXT | AOUT_SET_EXTERN, .value = 0 };
    aout_add_symbol(obj, "main", &sym);

    // Write to file
    aout_write_file_named(obj, "output.o");

    aout_free(obj);
    return 0;
}
```

See `aout.h` for the full API and `test_aout.c` for comprehensive usage examples.

## Project Structure

- `aout.h`, `aout.c` — C library implementation
- `test_aout.c` — test suite (53 tests)
- `dump/` — `dump` CLI tool
- `strip/` — `strip` CLI tool
- `ld/` — `ld` linker CLI tool
- `ar/` — `ar` librarian CLI tool
- `hash/` — hash table submodule (used for symbol lookup)
- `testy/` — lightweight C test framework submodule
- `ref/` — C++ reference implementation

## License

See LICENSE file for details.
