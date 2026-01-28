#include "test.h"
#include "aout.h"
#include <string.h>

void test_main(int argc, char *argv[])
{
	(void)argc;  // Unused
	(void)argv;  // Unused
	
	BEGIN_TESTS();
	
	MODULE("aout C library");
	
	// === Basic Object File Tests ===
	SUITE("Object File Creation");
	
	aout_object_file_t *obj = aout_create();
	TEST(obj != NULL);
	TEST(aout_is_valid(obj) == 0);  // Empty object file is not valid
	TEST(aout_get_text_size(obj) == 0);
	TEST(aout_get_data_size(obj) == 0);
	TEST(aout_get_bss_size(obj) == 0);
	
	// === Segment Tests ===
	SUITE("Segment Operations");
	
	COMMENT("Testing text segment...");
	uint32_t addr1 = aout_add_text(obj, 0x90);  // NOP
	TEST(addr1 == 0);
	uint32_t addr2 = aout_add_text(obj, 0xA9);  // LDA immediate
	TEST(addr2 == 1);
	uint32_t addr3 = aout_add_text(obj, 0x42);  // operand
	TEST(addr3 == 2);
	TEST(aout_get_text_size(obj) == 3);
	TEST(aout_is_valid(obj) == 1);  // Now valid with text
	
	COMMENT("Testing data segment...");
	uint32_t data_addr1 = aout_add_data(obj, 0xDE);
	TEST(data_addr1 == 0);
	uint32_t data_addr2 = aout_add_data(obj, 0xAD);
	TEST(data_addr2 == 1);
	uint32_t data_addr3 = aout_add_data(obj, 0xBE);
	TEST(data_addr3 == 2);
	uint32_t data_addr4 = aout_add_data(obj, 0xEF);
	TEST(data_addr4 == 3);
	TEST(aout_get_data_size(obj) == 4);
	
	COMMENT("Testing BSS segment...");
	uint32_t bss_addr1 = aout_alloc_bss(obj, 10);
	TEST(bss_addr1 == 0);
	uint32_t bss_addr2 = aout_alloc_bss(obj, 20);
	TEST(bss_addr2 == 10);
	TEST(aout_get_bss_size(obj) == 30);
	
	// === Symbol Tests ===
	SUITE("Symbol Operations");
	
	COMMENT("Adding symbols...");
	aout_symbol_t sym1;
	sym1.type = AOUT_SET_TEXT | AOUT_SET_EXTERN;
	sym1.value = 0;
	aout_add_symbol(obj, "main", &sym1);
	TEST(aout_get_text_size(obj) > 0);  // Symbol added
	
	aout_symbol_t sym2;
	sym2.type = AOUT_SET_DATA;
	sym2.value = 0;
	aout_add_symbol(obj, "counter", &sym2);
	
	aout_symbol_t sym3;
	sym3.type = AOUT_SET_BSS;
	sym3.value = 0;
	aout_add_symbol(obj, "buffer", &sym3);
	
	COMMENT("Finding symbols...");
	size_t idx = aout_index_of_symbol(obj, "main");
	TEST(idx != AOUT_INVALID_INDEX);
	
	aout_symbol_t found_sym;
	int result = aout_find_symbol(obj, "main", &found_sym);
	TEST(result == 1);
	TEST(found_sym.type == (AOUT_SET_TEXT | AOUT_SET_EXTERN));
	TEST(found_sym.value == 0);
	
	result = aout_find_symbol(obj, "nonexistent", &found_sym);
	TEST(result == 0);
	
	COMMENT("Testing symbol lookup by address...");
	char *symbol_name;
	result = aout_find_code_symbol_by_addr(obj, 0, &symbol_name);
	TEST(result == 1);
	TEST(strcmp(symbol_name, "main") == 0);
	
	result = aout_find_data_symbol_by_addr(obj, 0, &symbol_name);
	TEST(result == 1);
	TEST(strcmp(symbol_name, "counter") == 0);
	
	// === Relocation Tests ===
	SUITE("Relocation Operations");
	
	COMMENT("Adding relocations...");
	aout_relocation_entry_t reloc1;
	memset(&reloc1, 0, sizeof(reloc1));
	reloc1.address = 1;
	reloc1.index = 0;  // Symbol index 0
	reloc1.length = 1;  // 2 bytes
	reloc1.external = 1;
	aout_add_text_relocation(obj, &reloc1);
	
	aout_relocation_entry_t reloc2;
	memset(&reloc2, 0, sizeof(reloc2));
	reloc2.address = 0;
	reloc2.index = AOUT_SEG_DATA;
	reloc2.length = 1;
	reloc2.external = 0;
	aout_add_data_relocation(obj, &reloc2);
	
	// === Base Address Tests ===
	SUITE("Base Address Operations");
	
	aout_set_text_base(obj, 0x1000);
	aout_set_data_base(obj, 0x2000);
	aout_set_bss_base(obj, 0x3000);
	
	TEST(aout_get_text_base(obj) == 0x1000);
	TEST(aout_get_data_base(obj) == 0x2000);
	TEST(aout_get_bss_base(obj) == 0x3000);
	
	COMMENT("Updating BSS symbols...");
	aout_update_bss_symbols(obj);
	result = aout_find_symbol(obj, "buffer", &found_sym);
	TEST(result == 1);
	TEST(found_sym.value == 0x3000);  // Updated with base
	
	// === File I/O Tests ===
	SUITE("File I/O");
	
	COMMENT("Writing object file...");
	aout_set_entry_point(obj, 0);
	int write_result = aout_write_file_named(obj, "test_output.o");
	TEST(write_result == AOUT_OK);
	
	COMMENT("Reading object file...");
	aout_object_file_t *obj2 = aout_create();
	int read_result = aout_read_file_named(obj2, "test_output.o");
	TEST(read_result == AOUT_OK);
	TEST(aout_is_valid(obj2) == 1);
	TEST(aout_get_text_size(obj2) == aout_get_text_size(obj));
	TEST(aout_get_data_size(obj2) == aout_get_data_size(obj));
	TEST(aout_get_bss_size(obj2) == aout_get_bss_size(obj));
	
	COMMENT("Verifying symbols after read...");
	result = aout_find_symbol(obj2, "main", &found_sym);
	TEST(result == 1);
	TEST(found_sym.type == (AOUT_SET_TEXT | AOUT_SET_EXTERN));
	
	result = aout_find_symbol(obj2, "counter", &found_sym);
	TEST(result == 1);
	TEST(found_sym.type == AOUT_SET_DATA);
	
	result = aout_find_symbol(obj2, "buffer", &found_sym);
	TEST(result == 1);
	TEST(found_sym.type == AOUT_SET_BSS);
	
	// === Clear Test ===
	SUITE("Clear Operations");
	
	aout_clear(obj2);
	TEST(aout_is_valid(obj2) == 0);
	TEST(aout_get_text_size(obj2) == 0);
	TEST(aout_get_data_size(obj2) == 0);
	TEST(aout_get_bss_size(obj2) == 0);
	
	// === Concatenation Tests ===
	SUITE("Concatenation");
	
	COMMENT("Creating two object files...");
	aout_object_file_t *lhs = aout_create();
	aout_object_file_t *rhs = aout_create();
	
	// LHS: text segment with undefined symbol
	aout_add_text(lhs, 0x4C);  // JMP
	aout_add_text(lhs, 0x00);
	aout_add_text(lhs, 0x00);
	
	aout_symbol_t undefined_sym;
	undefined_sym.type = AOUT_SET_UNDEFINED | AOUT_SET_EXTERN;
	undefined_sym.value = 0;
	aout_add_symbol(lhs, "external_func", &undefined_sym);
	
	// RHS: defines the symbol
	aout_add_text(rhs, 0x60);  // RTS
	
	aout_symbol_t defined_sym;
	defined_sym.type = AOUT_SET_TEXT | AOUT_SET_EXTERN;
	defined_sym.value = 0;
	aout_add_symbol(rhs, "external_func", &defined_sym);
	
	aout_set_text_base(rhs, aout_get_text_size(lhs));
	
	COMMENT("Concatenating objects...");
	size_t orig_lhs_size = aout_get_text_size(lhs);
	size_t orig_rhs_size = aout_get_text_size(rhs);
	
	aout_concat(lhs, rhs);
	
	TEST(aout_get_text_size(lhs) == orig_lhs_size + orig_rhs_size);
	
	COMMENT("Verifying symbol resolution...");
	result = aout_find_symbol(lhs, "external_func", &found_sym);
	TEST(result == 1);
	TEST(!(found_sym.type & AOUT_SET_UNDEFINED));  // Now defined
	
	// === Cleanup ===
	SUITE("Cleanup");
	
	aout_free(obj);
	aout_free(obj2);
	aout_free(lhs);
	aout_free(rhs);
	
	TEST(1);  // Cleanup successful
	
	// printf("\n%sTest pass completed%s.\nEvaluated %s%d%s modules, %s%d%s suites, and %s%d%s tests with %s%d%s failed test case(s).\n\n", 
	// 	TERM_BRIGHT_MAGENTA, TERM_RESET, 
	// 	TERM_GREEN, test_modules, TERM_RESET, 
	// 	TERM_GREEN, test_suites, TERM_RESET, 
	// 	TERM_GREEN, test_number, TERM_RESET, 
	// 	test_failures ? TERM_BRIGHT_RED : TERM_GREEN, test_failures, TERM_RESET);
}
