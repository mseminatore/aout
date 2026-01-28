# aout

A C/C++ library for reading, writing, and manipulating Unix a.out object files.

## Features

- Read and write a.out format object files
- Manipulate text, data, and BSS segments
- Symbol table management with fast lookup
- Relocation entry handling
- Object file concatenation and linking
- Debug output and hex dumps

## Building

### C Version

```bash
make
make test
```

### Running Tests

```bash
make test
```

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
```

### C++ API Example

See `ref/aout.h` and `ref/aout.cpp` for the C++ reference implementation.

## Project Structure

- `aout.h`, `aout.c` - C implementation
- `ref/` - C++ reference implementation  
- `hash/` - Hash table submodule (used for symbol lookup)
- `testy/` - Testing framework submodule
- `test_aout.c` - Test suite

## License

See LICENSE file for details.