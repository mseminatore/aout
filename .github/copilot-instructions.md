# AI Coding Agent Instructions for aout

## Project Overview
**aout** is a C/C++ library for reading, writing, and manipulating Unix a.out object files. The core functionality handles binary object file formats including headers, segment data (text/data/BSS), symbol tables, string tables, and relocation entries.

**Two Implementations:**
- **C version** (`aout.h`, `aout.c`) - Primary implementation using hash tables and dynamic arrays
- **C++ reference** (`ref/aout.h`, `ref/aout.cpp`) - Reference implementation using STL containers

## Architecture & Core Components

### File Format Structure (`ref/aout.h`, `ref/aout.cpp`)
- **AOUT_HEADER**: 32-byte header containing magic number, segment sizes, entry point, and relocation sizes
- **Three Segments**: 
  - TEXT (code)
  - DATA (initialized data)
  - BSS (uninitialized data - size only, no content)
- **Symbol System**: Name-indexed symbol table with bidirectional lookups (name→index, address→name) for TEXT/DATA/BSS segments
- **Relocation Entries**: 8-byte structs tracking address patches needed for external symbol resolution
- **String Table**: Packed null-terminated strings referenced by symbol nameOffsets

### C Implementation (`aout.h`, `aout.c`)
- **API Naming**: Snake_case with `aout_` prefix (e.g., `aout_add_symbol()`, `aout_create()`)
- **Struct Types**: Consistent `_t` suffix (e.g., `aout_object_file_t`, `aout_symbol_t`)
- **Symbol Lookup**: Hash table from `hash/` submodule with `HT_HASH_STRING` for O(1) name lookups
- **Reverse Lookups**: Sorted arrays of (address, name) pairs using binary search
- **Dynamic Arrays**: Type-specific implementations (byte_array_t, reloc_array_t, symbol_table_t) with automatic growth
- **Memory Management**: Explicit allocation/deallocation via `aout_create()` / `aout_free()`
- **Error Handling**: Return codes (AOUT_OK, AOUT_ERROR_*) for operations, assertions for invariants

### Key Data Structures
- `SymbolTable`: Vector of (name, SymbolEntity) pairs with `SymbolLookup` map for O(1) name access
- `SymbolRLookup` maps: Separate reverse lookups for each segment type (code/data/BSS) by address
- **Byte-level access**: Use `LOBYTE()`/`HIBYTE()` macros for 16-bit integer packing into segments

### Critical Workflows

#### Symbol Resolution & Relocation (`concat()`, `relocate()`)
1. **Concatenation** merges multiple object files:
   - Undefined symbols in LHS resolved from RHS defined symbols
   - Text/data/BSS addresses adjusted by segment bases
   - Relocation entries updated with new offsets
2. **Relocation** patches external references:
   - Iterate textRelocs/dataRelocs looking for `external=1` entries
   - Look up symbol address from all modules
   - Write resolved address directly to segment using LOBYTE/HIBYTE

#### Initialization Workflow
- Always call `clear()` before reusing ObjectFile to reset all segments, symbols, string table
- Set segment bases (`setTextBase()`, `setDataBase()`, `setBssBase()`) before relocation/lookup
- Call `updateBssSymbols()` after setting BSS base to update symbol values with base offset

### Development Patterns
- **Assertions over exceptions**: Code uses `assert()` for invariant validation, not exception handling
- **Binary layout awareness**: Fixed struct sizes validated with `static_assert` (AOUT_HEADER=32, RelocationEntry=8, SymbolEntity=12 bytes)
- **Segment normalization**: BSS size stored in header but segment itself is empty; BSS symbols track allocated space
- **String table optimization**: Symbol names stored once in stringTable; nameOffset points to null-terminated string

## Avoid
- Changing struct layouts without updating static_assert validations
- Modifying segment data without updating corresponding header sizes (a_text, a_data, a_bss, a_trsize, a_drsize)
- Assuming BSS has content; allocBSS() only updates header size
- Direct symbol lookups without checking SET_UNDEFINED flag to verify symbol definition

## Submodules

### hash (`hash/` directory)
- Open-addressing hash table library in C with collision resistance
- Uses opaque `ht_key_t` and `ht_value_t` pointers (caller owns allocation/deallocation)
- Pluggable hash and compare functions; built-in `HT_HASH_STRING` for string keys
- Default hash uses pointer values; default compare uses pointer equality (set custom compare for strings via `ht_set_compare_func`)
- API: `ht_create()`, `ht_insert()`, `ht_find()`, `ht_remove()`, `ht_free()`
- Stats tracking available: collision counts and load factor management

### testy (`testy/` directory)
- Lightweight C/C++ test framework with VT100 colored output
- Integrate by including `test.h` and linking `test_main.c`
- Implement `void test_main(int argc, char *argv[])` for tests
- Macros: `MODULE()`, `SUITE()`, `TEST()`, `TESTEX()`, `COMMENT()`, `EQUAL_EPSILON()`, `EQUAL_ARRAY()`
- Automatic failure tracking and reporting

## Build & Testing
- C++11 compatible (standard includes: string, vector, map)
- Platform note: `_CRT_SECURE_NO_WARNINGS` defined for MSVC; uses FILE* for binary I/O
- No external dependencies beyond STL
- Submodules provide optional capabilities: hash table library and testing framework
