#include "aout.h"
#include "hash/hash.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>

// === Dynamic Array Implementations ===

// Byte array functions
static byte_array_t* byte_array_create(void)
{
	byte_array_t *arr = (byte_array_t*)malloc(sizeof(byte_array_t));
	if (!arr) return NULL;
	
	arr->data = NULL;
	arr->size = 0;
	arr->capacity = 0;
	return arr;
}

static void byte_array_free(byte_array_t *arr)
{
	if (!arr) return;
	free(arr->data);
	free(arr);
}

static int byte_array_push(byte_array_t *arr, uint8_t item)
{
	if (arr->size >= arr->capacity)
	{
		size_t new_capacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
		uint8_t *new_data = (uint8_t*)realloc(arr->data, new_capacity);
		if (!new_data) return AOUT_ERROR_MEMORY;
		
		arr->data = new_data;
		arr->capacity = new_capacity;
	}
	
	arr->data[arr->size++] = item;
	return AOUT_OK;
}

static void byte_array_clear(byte_array_t *arr)
{
	arr->size = 0;
}

static void byte_array_insert_range(byte_array_t *arr, const uint8_t *items, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
		byte_array_push(arr, items[i]);
}

// Relocation array functions
static reloc_array_t* reloc_array_create(void)
{
	reloc_array_t *arr = (reloc_array_t*)malloc(sizeof(reloc_array_t));
	if (!arr) return NULL;
	
	arr->data = NULL;
	arr->size = 0;
	arr->capacity = 0;
	return arr;
}

static void reloc_array_free(reloc_array_t *arr)
{
	if (!arr) return;
	free(arr->data);
	free(arr);
}

static int reloc_array_push(reloc_array_t *arr, const aout_relocation_entry_t *item)
{
	if (arr->size >= arr->capacity)
	{
		size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
		aout_relocation_entry_t *new_data = (aout_relocation_entry_t*)realloc(
			arr->data, new_capacity * sizeof(aout_relocation_entry_t));
		if (!new_data) return AOUT_ERROR_MEMORY;
		
		arr->data = new_data;
		arr->capacity = new_capacity;
	}
	
	arr->data[arr->size++] = *item;
	return AOUT_OK;
}

static void reloc_array_clear(reloc_array_t *arr)
{
	arr->size = 0;
}

// Symbol table functions
static symbol_table_t* symbol_table_create(void)
{
	symbol_table_t *tbl = (symbol_table_t*)malloc(sizeof(symbol_table_t));
	if (!tbl) return NULL;
	
	tbl->data = NULL;
	tbl->size = 0;
	tbl->capacity = 0;
	return tbl;
}

static void symbol_table_free(symbol_table_t *tbl)
{
	size_t i;
	if (!tbl) return;
	
	for (i = 0; i < tbl->size; i++)
		free(tbl->data[i].name);
	
	free(tbl->data);
	free(tbl);
}

static int symbol_table_push(symbol_table_t *tbl, const char *name, const aout_symbol_t *sym)
{
	if (tbl->size >= tbl->capacity)
	{
		size_t new_capacity = tbl->capacity == 0 ? 16 : tbl->capacity * 2;
		symbol_entry_t *new_data = (symbol_entry_t*)realloc(
			tbl->data, new_capacity * sizeof(symbol_entry_t));
		if (!new_data) return AOUT_ERROR_MEMORY;
		
		tbl->data = new_data;
		tbl->capacity = new_capacity;
	}
	
	tbl->data[tbl->size].name = strdup(name);
	if (!tbl->data[tbl->size].name) return AOUT_ERROR_MEMORY;
	
	tbl->data[tbl->size].symbol = *sym;
	tbl->size++;
	return AOUT_OK;
}

static void symbol_table_clear(symbol_table_t *tbl)
{
	size_t i;
	for (i = 0; i < tbl->size; i++)
		free(tbl->data[i].name);
	tbl->size = 0;
}

// Address-name array functions
static addr_name_array_t* addr_name_array_create(void)
{
	addr_name_array_t *arr = (addr_name_array_t*)malloc(sizeof(addr_name_array_t));
	if (!arr) return NULL;
	
	arr->data = NULL;
	arr->size = 0;
	arr->capacity = 0;
	return arr;
}

static void addr_name_array_free(addr_name_array_t *arr)
{
	if (!arr) return;
	free(arr->data);
	free(arr);
}

static int addr_name_pair_compare(const void *a, const void *b)
{
	const addr_name_pair_t *pa = (const addr_name_pair_t*)a;
	const addr_name_pair_t *pb = (const addr_name_pair_t*)b;
	
	if (pa->address < pb->address) return -1;
	if (pa->address > pb->address) return 1;
	return 0;
}

static int addr_name_array_insert(addr_name_array_t *arr, uint32_t address, const char *name)
{
	if (arr->size >= arr->capacity)
	{
		size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
		addr_name_pair_t *new_data = (addr_name_pair_t*)realloc(
			arr->data, new_capacity * sizeof(addr_name_pair_t));
		if (!new_data) return AOUT_ERROR_MEMORY;
		
		arr->data = new_data;
		arr->capacity = new_capacity;
	}
	
	arr->data[arr->size].address = address;
	arr->data[arr->size].name = (char*)name;
	arr->size++;
	
	// Keep sorted
	qsort(arr->data, arr->size, sizeof(addr_name_pair_t), addr_name_pair_compare);
	return AOUT_OK;
}

static int addr_name_array_find(addr_name_array_t *arr, uint32_t address, char **name)
{
	addr_name_pair_t key;
	addr_name_pair_t *result;
	
	key.address = address;
	result = (addr_name_pair_t*)bsearch(&key, arr->data, arr->size, 
		sizeof(addr_name_pair_t), addr_name_pair_compare);
	
	if (result)
	{
		*name = result->name;
		return 1;
	}
	
	return 0;
}

static int addr_name_array_find_nearest(addr_name_array_t *arr, uint32_t address, 
	char **name, uint16_t *sym_addr)
{
	size_t i;
	if (arr->size == 0) return 0;
	
	// Find the first entry >= address
	for (i = 0; i < arr->size; i++)
	{
		if (arr->data[i].address >= address)
		{
			// If exact match or first entry, use it
			if (arr->data[i].address == address || i == 0)
			{
				*name = arr->data[i].name;
				*sym_addr = (uint16_t)arr->data[i].address;
				return 1;
			}
			// Otherwise use previous entry
			*name = arr->data[i-1].name;
			*sym_addr = (uint16_t)arr->data[i-1].address;
			return 1;
		}
	}
	
	// All entries are < address, use last one
	*name = arr->data[arr->size-1].name;
	*sym_addr = (uint16_t)arr->data[arr->size-1].address;
	return 1;
}

static void addr_name_array_clear(addr_name_array_t *arr)
{
	arr->size = 0;
}

// === Hash table helper functions ===

static int string_compare(ht_key_t a, ht_key_t b)
{
	return strcmp((const char*)a, (const char*)b) == 0;
}

// === Object File Implementation ===

aout_object_file_t* aout_create(void)
{
	aout_object_file_t *obj = (aout_object_file_t*)malloc(sizeof(aout_object_file_t));
	if (!obj) return NULL;
	
	memset(&obj->header, 0, sizeof(obj->header));
	obj->header.a_magic = 263; // NMAGIC
	
	obj->filename = NULL;
	
	obj->text_segment = byte_array_create();
	obj->data_segment = byte_array_create();
	obj->text_relocs = reloc_array_create();
	obj->data_relocs = reloc_array_create();
	obj->symbol_table = symbol_table_create();
	obj->string_table = byte_array_create();
	
	obj->code_symbol_rlookup = addr_name_array_create();
	obj->data_symbol_rlookup = addr_name_array_create();
	obj->bss_symbol_rlookup = addr_name_array_create();
	
	obj->symbol_lookup = ht_create();
	ht_set_hash_func((HashTable*)obj->symbol_lookup, HT_HASH_STRING);
	ht_set_compare_func((HashTable*)obj->symbol_lookup, string_compare);
	
	obj->text_base = 0;
	obj->data_base = 0;
	obj->bss_base = 0;
	
	return obj;
}

void aout_free(aout_object_file_t *obj)
{
	if (!obj) return;
	
	free(obj->filename);
	byte_array_free(obj->text_segment);
	byte_array_free(obj->data_segment);
	reloc_array_free(obj->text_relocs);
	reloc_array_free(obj->data_relocs);
	symbol_table_free(obj->symbol_table);
	byte_array_free(obj->string_table);
	
	addr_name_array_free(obj->code_symbol_rlookup);
	addr_name_array_free(obj->data_symbol_rlookup);
	addr_name_array_free(obj->bss_symbol_rlookup);
	
	ht_free((HashTable*)obj->symbol_lookup);
	
	free(obj);
}

void aout_clear(aout_object_file_t *obj)
{
	if (!obj) return;
	
	memset(&obj->header, 0, sizeof(obj->header));
	obj->header.a_magic = 263;
	
	byte_array_clear(obj->text_segment);
	byte_array_clear(obj->data_segment);
	reloc_array_clear(obj->text_relocs);
	reloc_array_clear(obj->data_relocs);
	symbol_table_clear(obj->symbol_table);
	byte_array_clear(obj->string_table);
	
	addr_name_array_clear(obj->code_symbol_rlookup);
	addr_name_array_clear(obj->data_symbol_rlookup);
	addr_name_array_clear(obj->bss_symbol_rlookup);
	
	/* Clear symbol lookup by recreating it */
	ht_free((HashTable*)obj->symbol_lookup);
	obj->symbol_lookup = ht_create();
	ht_set_hash_func((HashTable*)obj->symbol_lookup, HT_HASH_STRING);
	ht_set_compare_func((HashTable*)obj->symbol_lookup, string_compare);
	
	obj->text_base = 0;
	obj->data_base = 0;
	obj->bss_base = 0;
}

int aout_is_valid(const aout_object_file_t *obj)
{
	return obj && obj->header.a_magic == 263 && obj->text_segment->size > 0;
}

// === Segment operations ===

uint32_t aout_add_text(aout_object_file_t *obj, uint8_t item)
{
	uint32_t addr = obj->text_segment->size;
	byte_array_push(obj->text_segment, item);
	return addr;
}

uint32_t aout_add_data(aout_object_file_t *obj, uint8_t item)
{
	uint32_t addr = obj->data_segment->size;
	byte_array_push(obj->data_segment, item);
	return addr;
}

uint32_t aout_alloc_bss(aout_object_file_t *obj, size_t size)
{
	uint32_t addr = obj->header.a_bss;
	obj->header.a_bss += size;
	return addr;
}

uint32_t aout_get_text_size(const aout_object_file_t *obj)
{
	return obj->text_segment->size;
}

uint32_t aout_get_data_size(const aout_object_file_t *obj)
{
	return obj->data_segment->size;
}

uint32_t aout_get_bss_size(const aout_object_file_t *obj)
{
	return obj->header.a_bss;
}

uint32_t aout_get_entry_point(const aout_object_file_t *obj)
{
	return obj->header.a_entry;
}

void aout_set_text_base(aout_object_file_t *obj, uint32_t base)
{
	obj->text_base = base;
}

void aout_set_data_base(aout_object_file_t *obj, uint32_t base)
{
	obj->data_base = base;
}

void aout_set_bss_base(aout_object_file_t *obj, uint32_t base)
{
	obj->bss_base = base;
}

uint32_t aout_get_text_base(const aout_object_file_t *obj)
{
	return obj->text_base;
}

uint32_t aout_get_data_base(const aout_object_file_t *obj)
{
	return obj->data_base;
}

uint32_t aout_get_bss_base(const aout_object_file_t *obj)
{
	return obj->bss_base;
}

uint8_t* aout_text_ptr(aout_object_file_t *obj)
{
	return obj->text_segment->data;
}

uint8_t* aout_data_ptr(aout_object_file_t *obj)
{
	return obj->data_segment->data;
}

void aout_set_entry_point(aout_object_file_t *obj, uint16_t val)
{
	obj->header.a_entry = val;
}

// === Symbol operations ===

uint32_t aout_add_string(aout_object_file_t *obj, const char *name)
{
	uint32_t offset = obj->string_table->size;
	size_t len = strlen(name);
	size_t i;
	
	for (i = 0; i < len; i++)
		byte_array_push(obj->string_table, (uint8_t)name[i]);
	
	// null terminator
	byte_array_push(obj->string_table, 0);
	
	return offset;
}

void aout_add_symbol(aout_object_file_t *obj, const char *name, aout_symbol_t *sym)
{
	size_t *index_ptr;
	size_t index;
	
	// Check if symbol already exists
	index_ptr = (size_t*)ht_find((HashTable*)obj->symbol_lookup, name);
	
	if (index_ptr)
	{
		// Symbol exists - update it (for EXTERN resolution)
		index = *index_ptr;
		obj->symbol_table->data[index].symbol.type = sym->type;
		obj->symbol_table->data[index].symbol.value = sym->value;
		return;
	}
	
	// Add new symbol
	sym->name_offset = aout_add_string(obj, name);
	index = obj->symbol_table->size;
	
	symbol_table_push(obj->symbol_table, name, sym);
	
	// Add to lookup hash table
	index_ptr = (size_t*)malloc(sizeof(size_t));
	*index_ptr = index;
	ht_insert((HashTable*)obj->symbol_lookup, obj->symbol_table->data[index].name, index_ptr);
	
	// Add to reverse lookup
	if (sym->type & AOUT_SET_TEXT)
		addr_name_array_insert(obj->code_symbol_rlookup, sym->value, obj->symbol_table->data[index].name);
	else if (sym->type & AOUT_SET_DATA)
		addr_name_array_insert(obj->data_symbol_rlookup, sym->value, obj->symbol_table->data[index].name);
	else if (sym->type & AOUT_SET_BSS)
		addr_name_array_insert(obj->bss_symbol_rlookup, sym->value, obj->symbol_table->data[index].name);
}

size_t aout_index_of_symbol(aout_object_file_t *obj, const char *name)
{
	size_t *index_ptr = (size_t*)ht_find((HashTable*)obj->symbol_lookup, name);
	return index_ptr ? *index_ptr : AOUT_INVALID_INDEX;
}

aout_symbol_t aout_symbol_at(aout_object_file_t *obj, size_t index)
{
	return obj->symbol_table->data[index].symbol;
}

int aout_find_symbol(aout_object_file_t *obj, const char *name, aout_symbol_t *sym)
{
	size_t index = aout_index_of_symbol(obj, name);
	if (index != AOUT_INVALID_INDEX)
	{
		*sym = aout_symbol_at(obj, index);
		return 1;
	}
	return 0;
}

int aout_find_code_symbol_by_addr(aout_object_file_t *obj, uint16_t addr, char **name)
{
	*name = "<none>";
	return addr_name_array_find(obj->code_symbol_rlookup, addr, name);
}

int aout_find_data_symbol_by_addr(aout_object_file_t *obj, uint16_t addr, char **name)
{
	*name = "<none>";
	
	if (addr_name_array_find(obj->data_symbol_rlookup, addr, name))
		return 1;
	
	if (addr_name_array_find(obj->bss_symbol_rlookup, addr, name))
		return 1;
	
	return 0;
}

int aout_find_nearest_code_symbol_to_addr(aout_object_file_t *obj, uint16_t addr, 
	char **name, uint16_t *sym_addr)
{
	return addr_name_array_find_nearest(obj->code_symbol_rlookup, addr, name, sym_addr);
}

void aout_update_bss_symbols(aout_object_file_t *obj)
{
	size_t i;
	for (i = 0; i < obj->symbol_table->size; i++)
	{
		if (obj->symbol_table->data[i].symbol.type & AOUT_SET_BSS)
			obj->symbol_table->data[i].symbol.value += aout_get_bss_base(obj);
	}
}

// === Relocation operations ===

void aout_add_text_relocation(aout_object_file_t *obj, aout_relocation_entry_t *r)
{
	reloc_array_push(obj->text_relocs, r);
}

void aout_add_data_relocation(aout_object_file_t *obj, aout_relocation_entry_t *r)
{
	reloc_array_push(obj->data_relocs, r);
}

uint32_t aout_get_text_reloc_size(const aout_object_file_t *obj)
{
	return obj->header.a_trsize;
}

uint32_t aout_get_data_reloc_size(const aout_object_file_t *obj)
{
	return obj->header.a_drsize;
}

void aout_strip_symbols(aout_object_file_t *obj)
{
	symbol_table_clear(obj->symbol_table);
	byte_array_clear(obj->string_table);
}

void aout_strip_relocations(aout_object_file_t *obj)
{
	reloc_array_clear(obj->text_relocs);
	reloc_array_clear(obj->data_relocs);
}

// === Concatenation ===

void aout_concat(aout_object_file_t *lhs, aout_object_file_t *rhs)
{
	size_t i;
	
	// Combine headers
	lhs->header.a_bss += rhs->header.a_bss;
	lhs->header.a_data += rhs->header.a_data;
	lhs->header.a_text += rhs->header.a_text;
	lhs->header.a_drsize += rhs->header.a_drsize;
	lhs->header.a_trsize += rhs->header.a_trsize;
	
	// Merge text and data segments
	byte_array_insert_range(lhs->text_segment, rhs->text_segment->data, rhs->text_segment->size);
	byte_array_insert_range(lhs->data_segment, rhs->data_segment->data, rhs->data_segment->size);
	
	// Merge symbols
	for (i = 0; i < rhs->symbol_table->size; i++)
	{
		aout_symbol_t se;
		size_t index;
		const char *name = rhs->symbol_table->data[i].name;
		
		if (aout_find_symbol(lhs, name, &se) && (se.type & AOUT_SET_UNDEFINED))
		{
			// Symbol exists in lhs as undefined - update it
			index = aout_index_of_symbol(lhs, name);
			assert(index != AOUT_INVALID_INDEX);
			
			lhs->symbol_table->data[index].symbol.type = rhs->symbol_table->data[i].symbol.type;
			
			if (lhs->symbol_table->data[index].symbol.type & AOUT_SET_TEXT)
				lhs->symbol_table->data[index].symbol.value = rhs->symbol_table->data[i].symbol.value + aout_get_text_base(rhs);
			else if (lhs->symbol_table->data[index].symbol.type & AOUT_SET_DATA)
				lhs->symbol_table->data[index].symbol.value = rhs->symbol_table->data[i].symbol.value + aout_get_data_base(rhs);
			else if (lhs->symbol_table->data[index].symbol.type & AOUT_SET_BSS)
				lhs->symbol_table->data[index].symbol.value = rhs->symbol_table->data[i].symbol.value + aout_get_bss_base(rhs);
		}
		else if (!(rhs->symbol_table->data[i].symbol.type & AOUT_SET_UNDEFINED))
		{
			// Symbol doesn't exist in lhs and is defined in rhs - add it
			aout_symbol_t sym = rhs->symbol_table->data[i].symbol;
			
			if (sym.type & AOUT_SET_TEXT)
				sym.value = rhs->symbol_table->data[i].symbol.value + aout_get_text_base(rhs);
			else if (sym.type & AOUT_SET_DATA)
				sym.value = rhs->symbol_table->data[i].symbol.value + aout_get_data_base(rhs);
			else if (sym.type & AOUT_SET_BSS)
				sym.value = rhs->symbol_table->data[i].symbol.value + aout_get_bss_base(rhs);
			else
				assert(0);
			
			aout_add_symbol(lhs, name, &sym);
		}
	}
	
	// Fixup relocations in lhs
	for (i = 0; i < lhs->text_relocs->size; i++)
	{
		if (lhs->text_relocs->data[i].external)
		{
			aout_symbol_t *sym = &lhs->symbol_table->data[lhs->text_relocs->data[i].index].symbol;
			
			if (!(sym->type & AOUT_SET_EXTERN))
			{
				lhs->text_relocs->data[i].address = sym->value;
				lhs->text_relocs->data[i].external = 0;
				
				if (sym->type & AOUT_SET_TEXT)
					lhs->text_relocs->data[i].index = AOUT_SEG_TEXT;
				else if (sym->type & AOUT_SET_DATA)
					lhs->text_relocs->data[i].index = AOUT_SEG_DATA;
				else if (sym->type & AOUT_SET_BSS)
					lhs->text_relocs->data[i].index = AOUT_SEG_BSS;
				else
					assert(0);
			}
		}
	}
	
	// Merge relocations from rhs
	for (i = 0; i < rhs->text_relocs->size; i++)
	{
		if (rhs->text_relocs->data[i].external)
			continue;
		
		aout_relocation_entry_t re = rhs->text_relocs->data[i];
		
		if (re.index == AOUT_SEG_TEXT)
			re.address += aout_get_text_base(rhs);
		else if (re.index == AOUT_SEG_DATA)
			re.address += aout_get_data_base(rhs);
		else if (re.index == AOUT_SEG_BSS)
			re.address += aout_get_bss_base(rhs);
		else
			assert(0);
		
		reloc_array_push(lhs->text_relocs, &re);
	}
}

// === Relocation ===

/* Read up to 4 bytes little-endian from buf */
static uint32_t read_le(const uint8_t *buf, int bytes)
{
	uint32_t val = 0;
	int b;
	for (b = 0; b < bytes; b++)
		val |= ((uint32_t)buf[b]) << (8 * b);
	return val;
}

/* Write up to 4 bytes little-endian to buf */
static void write_le(uint8_t *buf, uint32_t val, int bytes)
{
	int b;
	for (b = 0; b < bytes; b++)
		buf[b] = (val >> (8 * b)) & 0xFF;
}

int aout_relocate(aout_object_file_t *obj, aout_object_file_t **modules, size_t module_count)
{
	size_t i, j;
	
	// Process text segment relocations
	for (i = 0; i < obj->text_relocs->size; i++)
	{
		aout_relocation_entry_t re = obj->text_relocs->data[i];
		int patch_size = 1 << re.length;  /* 1, 2, 4, or 8 bytes */
		int symbol_found = 0;
		
		if (re.external)
		{
			const char *name = obj->symbol_table->data[re.index].name;
			
			// Find external symbol in modules and patch
			for (j = 0; j < module_count; j++)
			{
				aout_symbol_t extern_sym;
				if (aout_find_symbol(modules[j], name, &extern_sym))
				{
					if (!(extern_sym.type & AOUT_SET_UNDEFINED))
					{
						uint32_t addr;
						uint32_t patch_val;
						
						if (extern_sym.type & AOUT_SET_TEXT)
							addr = aout_get_text_base(modules[j]) + extern_sym.value;
						else if (extern_sym.type & AOUT_SET_DATA)
							addr = aout_get_data_base(modules[j]) + extern_sym.value;
						else if (extern_sym.type & AOUT_SET_BSS)
							addr = aout_get_bss_base(modules[j]) + extern_sym.value;
						else
							assert(0);
						
						if (re.pcrel)
							patch_val = addr - (obj->text_base + re.address + (uint32_t)patch_size);
						else
							patch_val = addr;
						
						write_le(obj->text_segment->data + re.address, patch_val, patch_size);
						symbol_found++;
					}
				}
			}
			
			if (symbol_found == 0)
			{
				fprintf(stderr, "Error: Symbol '%s' not found!\n", name);
				return AOUT_ERROR_SYMBOL_NOT_FOUND;
			}
			else if (symbol_found > 1)
			{
				fprintf(stderr, "Error: Symbol '%s' multiply defined!\n", name);
				return AOUT_ERROR_SYMBOL_MULTIPLY_DEFINED;
			}
		}
		else
		{
			// Internal relocation: read stored segment-relative value, add base
			uint8_t *patch_ptr = obj->text_segment->data + re.address;
			uint32_t stored = read_le(patch_ptr, patch_size);
			uint32_t base = 0;
			uint32_t patch_val;
			
			if (re.index == AOUT_SEG_TEXT)
				base = obj->text_base;
			else if (re.index == AOUT_SEG_DATA)
				base = obj->data_base;
			else if (re.index == AOUT_SEG_BSS)
				base = obj->bss_base;
			
			uint32_t abs_addr = base + stored;
			
			if (re.pcrel)
				patch_val = abs_addr - (obj->text_base + re.address + (uint32_t)patch_size);
			else
				patch_val = abs_addr;
			
			write_le(patch_ptr, patch_val, patch_size);
		}
	}
	
	return AOUT_OK;
}

// === File I/O ===

int aout_write_file_named(aout_object_file_t *obj, const char *name)
{
	FILE *f = fopen(name, "wb");
	int result;
	
	if (!f) return AOUT_ERROR_FILE;
	
	result = aout_write_file(obj, f);
	fclose(f);
	
	if (result == AOUT_OK)
	{
		free(obj->filename);
		obj->filename = strdup(name);
	}
	
	return result;
}

int aout_write_file(aout_object_file_t *obj, FILE *fptr)
{
	size_t i;
	
	assert(fptr != NULL);
	if (!fptr) return AOUT_ERROR_FILE;
	
	// Update header sizes
	obj->header.a_text = obj->text_segment->size;
	obj->header.a_data = obj->data_segment->size;
	obj->header.a_trsize = obj->text_relocs->size;
	obj->header.a_drsize = obj->data_relocs->size;
	obj->header.a_syms = obj->symbol_table->size;
	
	// Write header
	fwrite(&obj->header, sizeof(obj->header), 1, fptr);
	
	// Write text segment
	fwrite(obj->text_segment->data, obj->text_segment->size, 1, fptr);
	
	// Write data segment
	fwrite(obj->data_segment->data, obj->data_segment->size, 1, fptr);
	
	// Write text relocations
	for (i = 0; i < obj->text_relocs->size; i++)
		fwrite(&obj->text_relocs->data[i], sizeof(aout_relocation_entry_t), 1, fptr);
	
	// Write data relocations
	for (i = 0; i < obj->data_relocs->size; i++)
		fwrite(&obj->data_relocs->data[i], sizeof(aout_relocation_entry_t), 1, fptr);
	
	// Write symbol table
	for (i = 0; i < obj->symbol_table->size; i++)
		fwrite(&obj->symbol_table->data[i].symbol, sizeof(aout_symbol_t), 1, fptr);
	
	// Write string table
	for (i = 0; i < obj->string_table->size; i++)
		fputc(obj->string_table->data[i], fptr);
	
	return AOUT_OK;
}

int aout_read_file_named(aout_object_file_t *obj, const char *name)
{
	FILE *f = fopen(name, "rb");
	int result;
	
	if (!f) return AOUT_ERROR_FILE;
	
	result = aout_read_file(obj, f);
	fclose(f);
	
	if (result == AOUT_OK)
	{
		free(obj->filename);
		obj->filename = strdup(name);
	}
	
	return result;
}

int aout_read_file(aout_object_file_t *obj, FILE *fptr)
{
	int i, c;
	aout_symbol_t *st;
	
	assert(fptr != NULL);
	if (!fptr) return AOUT_ERROR_FILE;
	
	aout_clear(obj);
	
	// Read header
	if (fread(&obj->header, sizeof(obj->header), 1, fptr) != 1)
		return AOUT_ERROR_FILE;
	
	// Read text segment
	for (i = 0; i < obj->header.a_text; i++)
	{
		c = fgetc(fptr);
		byte_array_push(obj->text_segment, (uint8_t)c);
	}
	
	// Read data segment
	for (i = 0; i < obj->header.a_data; i++)
	{
		c = fgetc(fptr);
		byte_array_push(obj->data_segment, (uint8_t)c);
	}
	
	// Read text relocations
	for (i = 0; i < obj->header.a_trsize; i++)
	{
		aout_relocation_entry_t re;
		memset(&re, 0, sizeof(re));
		fread(&re, sizeof(re), 1, fptr);
		reloc_array_push(obj->text_relocs, &re);
	}
	
	// Read data relocations
	for (i = 0; i < obj->header.a_drsize; i++)
	{
		aout_relocation_entry_t re;
		memset(&re, 0, sizeof(re));
		fread(&re, sizeof(re), 1, fptr);
		reloc_array_push(obj->data_relocs, &re);
	}
	
	// Read symbol table (temporary storage)
	st = (aout_symbol_t*)malloc(obj->header.a_syms * sizeof(aout_symbol_t));
	if (!st) return AOUT_ERROR_MEMORY;
	
	for (i = 0; i < obj->header.a_syms; i++)
		fread(&st[i], sizeof(aout_symbol_t), 1, fptr);
	
	// Read string table
	c = fgetc(fptr);
	while (c != EOF)
	{
		byte_array_push(obj->string_table, (uint8_t)c);
		c = fgetc(fptr);
	}
	
	// Build symbol table with names from string table
	for (i = 0; i < obj->header.a_syms; i++)
	{
		char *name = (char*)&obj->string_table->data[st[i].name_offset];
		size_t *index_ptr;
		
		symbol_table_push(obj->symbol_table, name, &st[i]);
		
		// Add to lookup
		index_ptr = (size_t*)malloc(sizeof(size_t));
		*index_ptr = i;
		ht_insert((HashTable*)obj->symbol_lookup, obj->symbol_table->data[i].name, index_ptr);
		
		// Add to reverse lookups
		if (st[i].type & AOUT_SET_TEXT)
			addr_name_array_insert(obj->code_symbol_rlookup, st[i].value, obj->symbol_table->data[i].name);
		else if (st[i].type & AOUT_SET_DATA)
			addr_name_array_insert(obj->data_symbol_rlookup, st[i].value, obj->symbol_table->data[i].name);
		else if (st[i].type & AOUT_SET_BSS)
			addr_name_array_insert(obj->bss_symbol_rlookup, st[i].value, obj->symbol_table->data[i].name);
	}
	
	free(st);
	return AOUT_OK;
}

// === Debug/Dump functions ===

static const char* get_segment_name(uint32_t seg)
{
	if (seg == AOUT_SEG_TEXT) return ".text";
	if (seg == AOUT_SEG_DATA) return ".data";
	if (seg == AOUT_SEG_BSS) return ".bss";
	
	assert(0);
	return "unknown!";
}

void aout_dump_header(aout_object_file_t *obj, FILE *f)
{
	assert(f != NULL);
	if (!f) return;
	
	fprintf(f, "A.out File Header\n");
	fprintf(f, "-----------------\n\n");
	
	fprintf(f, "     Magic Number: " HEX_PREFIX "%X\n", obj->header.a_magic);
	fprintf(f, "Text Segment size: " HEX_PREFIX "%04X (%d) bytes\n", obj->header.a_text, obj->header.a_text);
	fprintf(f, "Data Segment size: " HEX_PREFIX "%04X (%d) bytes\n", obj->header.a_data, obj->header.a_data);
	fprintf(f, " BSS Segment size: " HEX_PREFIX "%04X (%d) bytes\n", obj->header.a_bss, obj->header.a_bss);
	fprintf(f, "Symbol Table size: " HEX_PREFIX "%04X (%d) bytes\n", obj->header.a_syms, obj->header.a_syms);
	fprintf(f, " Text reloc count: " HEX_PREFIX "%04X (%d) entries\n", obj->header.a_trsize, obj->header.a_trsize);
	fprintf(f, " Data reloc count: " HEX_PREFIX "%04X (%d) entries\n", obj->header.a_drsize, obj->header.a_drsize);
	fprintf(f, " Main Entry Point: " HEX_PREFIX "%04X\n\n", obj->header.a_entry);
}

void aout_hex_dump_group(FILE *f, uint8_t *buf)
{
	int i;
	for (i = 0; i < 4; i++)
		fprintf(f, "%02X ", buf[i]);
}

void aout_hex_dump_line(FILE *f, uint32_t offset, uint8_t *buf)
{
	int i;
	fprintf(f, "%04X: ", offset);
	
	aout_hex_dump_group(f, buf);
	fputc(' ', f);
	aout_hex_dump_group(f, buf+4);
	
	fputs("- ", f);
	
	aout_hex_dump_group(f, buf+8);
	fputc(' ', f);
	aout_hex_dump_group(f, buf+12);
	
	fputc('[', f);
	
	for (i = 0; i < 16; i++)
	{
		if (isprint(buf[i]))
			fputc(buf[i], f);
		else
			fputc('.', f);
	}
	
	fprintf(f, "]\n");
}

void aout_hex_dump_segment(FILE *f, uint8_t *seg, size_t size)
{
	uint32_t offset = 0;
	size_t i;
	uint8_t last_line[16];
	
	for (i = 0; i < size / 16; i++)
	{
		aout_hex_dump_line(f, offset, &seg[offset]);
		offset += 16;
	}
	
	if (0 == (size % 16))
		return;
	
	memset(last_line, 0, 16);
	memcpy(last_line, &seg[offset], size % 16);
	aout_hex_dump_line(f, offset, last_line);
}

void aout_dump_text(aout_object_file_t *obj, FILE *f)
{
	assert(f != NULL);
	if (!f) return;
	
	fprintf(f, ".text segment (hex)\n");
	fprintf(f, "-------------------\n\n");
	
	aout_hex_dump_segment(f, obj->text_segment->data, obj->text_segment->size);
	fputc('\n', f);
}

void aout_dump_data(aout_object_file_t *obj, FILE *f)
{
	assert(f != NULL);
	if (!f) return;
	
	fprintf(f, ".data segment (hex)\n");
	fprintf(f, "-------------------\n\n");
	
	aout_hex_dump_segment(f, obj->data_segment->data, obj->data_segment->size);
	fputc('\n', f);
}

void aout_dump_text_relocs(aout_object_file_t *obj, FILE *f)
{
	size_t i;
	
	assert(f != NULL);
	if (!f) return;
	
	if (obj->text_relocs->size == 0)
		return;
	
	fprintf(f, ".text segment relocations\n");
	fprintf(f, "-------------------------\n\n");
	
	for (i = 0; i < obj->text_relocs->size; i++)
	{
		aout_relocation_entry_t re = obj->text_relocs->data[i];
		if (re.external)
		{
			if (re.index < obj->symbol_table->size)
				fprintf(f, "%04X\tsize: %d (bytes)\tExternal\t%s\n",
					re.address, 1 << re.length, obj->symbol_table->data[re.index].name);
			else
				fprintf(f, "%04X\tsize: %d (bytes)\tExternal\t[index %u]\n",
					re.address, 1 << re.length, re.index);
		}
		else
			fprintf(f, "%04X\tsize: %d (bytes)\tSegment: %s\n", 
				re.address, 1 << re.length, get_segment_name(re.index));
	}
	
	fputc('\n', f);
}

void aout_dump_data_relocs(aout_object_file_t *obj, FILE *f)
{
	size_t i;
	
	assert(f != NULL);
	if (!f) return;
	
	if (obj->data_relocs->size == 0)
		return;
	
	fprintf(f, ".data segment relocations\n");
	fprintf(f, "-------------------------\n\n");
	
	for (i = 0; i < obj->data_relocs->size; i++)
	{
		aout_relocation_entry_t re = obj->data_relocs->data[i];
		fprintf(f, "%04X\texternal: %d\tsize: %d (bytes)\n", 
			re.address, re.external, 1 << re.length);
	}
	
	fputc('\n', f);
}

void aout_dump_symbols(aout_object_file_t *obj, FILE *f)
{
	size_t i;
	
	assert(f != NULL);
	if (!f) return;
	
	if (obj->symbol_table->size == 0)
		return;
	
	fprintf(f, "Symbols\n");
	fprintf(f, "-------\n\n");
	
	for (i = 0; i < obj->symbol_table->size; i++)
	{
		aout_symbol_t sym = obj->symbol_table->data[i].symbol;
		const char *name = obj->symbol_table->data[i].name;
		char type_str[128] = "";
		
		if (sym.type & AOUT_SET_EXTERN)
			strcat(type_str, " public");
		if (sym.type & AOUT_SET_TEXT)
			strcat(type_str, " .text");
		if (sym.type & AOUT_SET_DATA)
			strcat(type_str, " .data");
		if (sym.type & AOUT_SET_BSS)
			strcat(type_str, " .bss");
		if (sym.type & AOUT_SET_UNDEFINED)
			strcat(type_str, " external");
		
		fprintf(f, "%15s\t%s segment\toffset: %d (" HEX_PREFIX "%04X)\n", 
			name, type_str, sym.value, sym.value);
	}
}
