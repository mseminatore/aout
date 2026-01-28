#ifndef __AOUT_H
#define __AOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// we are not secure CRT compliant at this time
#define _CRT_SECURE_NO_WARNINGS

// byte-level access macros
#ifndef LOBYTE
#	define LOBYTE(val) ((val) & 0xFF)
#endif

#ifndef HIBYTE
#	define HIBYTE(val) (((val) & 0xFF00) >> 8)
#endif

#define HEX_PREFIX "$"

// Return codes
#define AOUT_OK 0
#define AOUT_ERROR -1
#define AOUT_ERROR_FILE -2
#define AOUT_ERROR_MEMORY -3
#define AOUT_ERROR_SYMBOL_NOT_FOUND -4
#define AOUT_ERROR_SYMBOL_MULTIPLY_DEFINED -5

// Invalid symbol index
#define AOUT_INVALID_INDEX ((size_t)-1)

// Define the a.out header
typedef struct aout_header_t
{
	int32_t a_magic;	// magic number
	int32_t a_text;		// text segment size
	int32_t a_data;		// initialized data size
	int32_t a_bss;		// uninitialized data size
	int32_t a_syms;		// symbol table size
	int32_t a_entry;	// entry point
	int32_t a_trsize;	// text relocation size
	int32_t a_drsize;	// data relocation size
} aout_header_t;

// Validate the size of the a.out struct (C89/C90 compatible assertion)
typedef char aout_header_size_check[(sizeof(aout_header_t) == 32) ? 1 : -1];

// Segment types for relocation entries
enum
{
	AOUT_SEG_TEXT,		// code segment
	AOUT_SEG_DATA,		// data segment
	AOUT_SEG_BSS		// uninitialized data segment
};

// Symbol Entity types
enum
{
	AOUT_SET_EXTERN = 1,		// globally visible symbol
	AOUT_SET_TEXT = 2,			// TEXT segment symbol
	AOUT_SET_DATA = 4,			// DATA segment symbol
	AOUT_SET_BSS = 8,			// BSS segment symbol
	AOUT_SET_ABS = 16,			// absolute non-relocatable item. Usually debugging symbol
	AOUT_SET_UNDEFINED = 32		// symbol not defined in this module
};

// Define the RelocationEntry type
typedef struct aout_relocation_entry_t
{
	uint32_t	address;		// offset within the segment (data or text) of the relocation item
	uint32_t	index : 24,		// if extern is true, index number into the symbol table of this item, otherwise it identifies which segment text/data/bss
				pcrel : 1,		// is the address relative to PC
				length : 2,		// byte size of the entry 0,1,2,3 = 1,2,4,8
				external : 1,	// is the symbol external to this segment
				spare : 4;		// unused
} aout_relocation_entry_t;

// Validate the size of the relocation entry struct
typedef char aout_relocation_entry_size_check[(sizeof(aout_relocation_entry_t) == 8) ? 1 : -1];

// Define the symbol entity struct
typedef struct aout_symbol_t
{
	uint32_t name_offset;	// offset in the string table of the null-terminated name of the symbol
	uint32_t type;			// one of the Symbol Entity Type (AOUT_SET_xxx) enumerations
	uint32_t value;			// address offset in the current segment
} aout_symbol_t;

// Validate the size of the symbol entity struct
typedef char aout_symbol_size_check[(sizeof(aout_symbol_t) == 12) ? 1 : -1];

// Forward declarations for dynamic array types
typedef struct byte_array_t byte_array_t;
typedef struct reloc_array_t reloc_array_t;
typedef struct symbol_table_t symbol_table_t;
typedef struct addr_name_array_t addr_name_array_t;

// Address-to-name pair for reverse lookup
typedef struct addr_name_pair_t
{
	uint32_t address;
	char *name;
} addr_name_pair_t;

// Define the object file structure
typedef struct aout_object_file_t
{
	aout_header_t header;
	char *filename;
	
	byte_array_t *text_segment;
	byte_array_t *data_segment;
	
	reloc_array_t *text_relocs;
	reloc_array_t *data_relocs;
	
	symbol_table_t *symbol_table;
	void *symbol_lookup;		// HashTable* - maps symbol name to index
	
	addr_name_array_t *code_symbol_rlookup;
	addr_name_array_t *data_symbol_rlookup;
	addr_name_array_t *bss_symbol_rlookup;
	
	byte_array_t *string_table;
	
	uint32_t text_base;
	uint32_t data_base;
	uint32_t bss_base;
} aout_object_file_t;

// Dynamic array structures
struct byte_array_t
{
	uint8_t *data;
	size_t size;
	size_t capacity;
};

struct reloc_array_t
{
	aout_relocation_entry_t *data;
	size_t size;
	size_t capacity;
};

// Symbol table entry (name + symbol entity)
typedef struct symbol_entry_t
{
	char *name;
	aout_symbol_t symbol;
} symbol_entry_t;

struct symbol_table_t
{
	symbol_entry_t *data;
	size_t size;
	size_t capacity;
};

struct addr_name_array_t
{
	addr_name_pair_t *data;
	size_t size;
	size_t capacity;
};

// === Object File API ===

// Create and destroy object file
aout_object_file_t* aout_create(void);
void aout_free(aout_object_file_t *obj);
void aout_clear(aout_object_file_t *obj);

// Validation
int aout_is_valid(const aout_object_file_t *obj);

// File I/O
int aout_write_file(aout_object_file_t *obj, FILE *fptr);
int aout_write_file_named(aout_object_file_t *obj, const char *name);
int aout_read_file(aout_object_file_t *obj, FILE *fptr);
int aout_read_file_named(aout_object_file_t *obj, const char *name);

// Stripping options
void aout_strip_symbols(aout_object_file_t *obj);
void aout_strip_relocations(aout_object_file_t *obj);

// Code/data segments
uint32_t aout_add_text(aout_object_file_t *obj, uint8_t item);
uint32_t aout_add_data(aout_object_file_t *obj, uint8_t item);
uint32_t aout_alloc_bss(aout_object_file_t *obj, size_t size);

uint32_t aout_get_text_size(const aout_object_file_t *obj);
uint32_t aout_get_data_size(const aout_object_file_t *obj);
uint32_t aout_get_bss_size(const aout_object_file_t *obj);
uint32_t aout_get_entry_point(const aout_object_file_t *obj);

void aout_set_text_base(aout_object_file_t *obj, uint32_t base);
void aout_set_data_base(aout_object_file_t *obj, uint32_t base);
void aout_set_bss_base(aout_object_file_t *obj, uint32_t base);
void aout_update_bss_symbols(aout_object_file_t *obj);

uint32_t aout_get_text_base(const aout_object_file_t *obj);
uint32_t aout_get_data_base(const aout_object_file_t *obj);
uint32_t aout_get_bss_base(const aout_object_file_t *obj);

uint8_t* aout_text_ptr(aout_object_file_t *obj);
uint8_t* aout_data_ptr(aout_object_file_t *obj);

void aout_set_entry_point(aout_object_file_t *obj, uint16_t val);

// Symbols
void aout_add_symbol(aout_object_file_t *obj, const char *name, aout_symbol_t *sym);
uint32_t aout_add_string(aout_object_file_t *obj, const char *name);
size_t aout_index_of_symbol(aout_object_file_t *obj, const char *name);
aout_symbol_t aout_symbol_at(aout_object_file_t *obj, size_t index);
int aout_find_symbol(aout_object_file_t *obj, const char *name, aout_symbol_t *sym);
int aout_find_code_symbol_by_addr(aout_object_file_t *obj, uint16_t addr, char **name);
int aout_find_data_symbol_by_addr(aout_object_file_t *obj, uint16_t addr, char **name);
int aout_find_nearest_code_symbol_to_addr(aout_object_file_t *obj, uint16_t addr, char **name, uint16_t *sym_addr);

// Relocations
void aout_add_text_relocation(aout_object_file_t *obj, aout_relocation_entry_t *r);
void aout_add_data_relocation(aout_object_file_t *obj, aout_relocation_entry_t *r);
int aout_relocate(aout_object_file_t *obj, aout_object_file_t **modules, size_t module_count);
void aout_concat(aout_object_file_t *lhs, aout_object_file_t *rhs);

uint32_t aout_get_text_reloc_size(const aout_object_file_t *obj);
uint32_t aout_get_data_reloc_size(const aout_object_file_t *obj);

// Debug output
void aout_dump_header(aout_object_file_t *obj, FILE *f);
void aout_dump_text(aout_object_file_t *obj, FILE *f);
void aout_dump_data(aout_object_file_t *obj, FILE *f);
void aout_dump_text_relocs(aout_object_file_t *obj, FILE *f);
void aout_dump_data_relocs(aout_object_file_t *obj, FILE *f);
void aout_dump_symbols(aout_object_file_t *obj, FILE *f);

// Helper functions
void aout_hex_dump_group(FILE *f, uint8_t *buf);
void aout_hex_dump_line(FILE *f, uint32_t offset, uint8_t *buf);
void aout_hex_dump_segment(FILE *f, uint8_t *seg, size_t size);

#ifdef __cplusplus
}
#endif

#endif // __AOUT_H
