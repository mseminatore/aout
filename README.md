# aout library

[![CMake](https://github.com/mseminatore/aout/actions/workflows/cmake.yml/badge.svg)](https://github.com/mseminatore/aout/actions/workflows/cmake.yml)

A C library for reading, writing, and manipulating Unix a.out object files.

## Features

- Read and write a.out format object files
- Manipulate text, data, and BSS segments
- Symbol table management with fast lookup
- Relocation entry handling
- Object file concatenation and linking
- Debug output and hex dumps

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

## Usage

### C API Example

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
    
    // Add a symbol
    aout_symbol_t sym;
    sym.type = AOUT_SET_TEXT | AOUT_SET_EXTERN;
    sym.value = 0;
    aout_add_symbol(obj, "main", &sym);
    
    // Write to file
    aout_write_file_named(obj, "output.o");
    
    // Clean up
    aout_free(obj);
    return 0;
}
``e `ref/aout.h` and `ref/aout.cpp` for the C++ reference implementation.

## Project Structure

- `aout.h`, `aout.c` - C implementation
- `ref/` - C++ reference implementation  
- `hash/` - Hash table submodule (used for symbol lookup)
- `test_aout.c` - Test suite

## License

See LICENSE file for details.
