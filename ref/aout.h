#ifndef __AOUT_H
#define __AOUT_H

#pragma once

// we are not secure CRT compliant at this time
#define _CRT_SECURE_NO_WARNINGS

// byte-level access macros
#ifndef LOBYTE
#	define LOBYTE(val) ((val) & 0xFF)
#endif

#ifndef HIBYTE
#	define HIBYTE(val) (((val) & 0xFF00) >> 8)
#endif

#include <string>
#include <vector>
#include <map>


#define HEX_PREFIX "$"
//#define HEX_PREFIX "0x"

// Define the a.out header
struct AOUT_HEADER
{
	int a_magic;	// magic number
	int a_text;		// text segment size
	int a_data;		// initialized data size
	int a_bss;		// uninitialized data size
	int a_syms;		// symbol table size
	int a_entry;	// entry point
	int a_trsize;	// text relocation size
	int a_drsize;	// data relocation size
};

// Validate the size of the a.out struct
static_assert(sizeof(AOUT_HEADER) == 32, "Invalid a.out header size!");

// Segment types for relocation entries
enum
{
	SEG_TEXT,		// code segment
	SEG_DATA,		// data segment
	SEG_BSS			// uninitialized data segment
};

// Symbol Entity types
enum
{
	SET_EXTERN = 1,		// globally visible symbol
	SET_TEXT = 2,		// TEXT segment symbol
	SET_DATA = 4,		// DATA segment symbol
	SET_BSS = 8,		// BSS segment symbol
	SET_ABS = 16,		// absolute non-relocatable item. Usually debugging symbol
	SET_UNDEFINED = 32	// symbol not defined in this module
};

// Define the RelocationEntry type
struct RelocationEntry
{
	uint32_t	address;		// offset within the segment (data or text) of the relocation item
	uint32_t	index : 24,		// if extern is true, index number into the symbol table of this item, otherwise it identifies which segment text/data/bss
				pcrel : 1,		// is the address relative to PC
				length : 2,		// byte size of the entry 0,1,2,3 = 1,2,4,8
				external : 1,	// is the symbol external to this segment
				spare : 4;		// unused
				//r_baserel : 1,
				//r_jmptable : 1,
				//r_relative : 1,
				//r_copy : 1;
	RelocationEntry()
	{
		address = index = pcrel = length = external = spare = 0;
	}
};

// Validate the size of the relocation entry struct
static_assert(sizeof(RelocationEntry) == 8, "Invalid relocation entry!");

// Define the symbol entity struct
struct SymbolEntity
{
	uint32_t nameOffset;	// offset in the string table of the null-terminated name of the symbol
	uint32_t type;			// one of the Symbol Entity Type (SET_xxx) enumerations
	uint32_t value;			// address offset in the current segment
};

// Validate the size of the symbol entity struct
static_assert(sizeof(SymbolEntity) == 12, "Invalid symbol entry!");

// Define the object file class
class ObjectFile
{
protected:
	AOUT_HEADER file_header;
	std::string filename;

	using Segment = std::vector<uint8_t>;
		
	Segment text_segment;
	Segment data_segment;

	using Relocations = std::vector<RelocationEntry>;
	Relocations textRelocs;
	Relocations dataRelocs;

	// Make this a vector with a map of name/index pairs for lookup.
	using SymbolTable = std::vector<std::pair<std::string, SymbolEntity> >;
	using SymbolLookup = std::map<std::string, size_t>;
	using SymbolRLookup = std::map<size_t, std::string>;

	SymbolTable symbolTable;
	SymbolLookup symbolLookup;
	SymbolRLookup codeSymbolRLookup;
	SymbolRLookup dataSymbolRLookup;
	SymbolRLookup bssSymbolRLookup;

	using StringTable = std::vector<char>;
	StringTable stringTable;

	uint32_t textBase, dataBase, bssBase;

public:
	ObjectFile();
	virtual ~ObjectFile();

	void clear();

	bool isValid() 
	{ 
		return file_header.a_magic == 263 && file_header.a_text > 0;
	}

	// file IO
	int writeFile(FILE *fptr);
	int writeFile(const std::string &name);
	int readFile(FILE *fptr);
	int readFile(const std::string &name);

	// stripping options
	void stripSymbols()		{ symbolTable.clear(); stringTable.clear();  }
	void stripRelocations() { textRelocs.clear(); dataRelocs.clear(); }

	// code/data segments
	uint32_t addText(uint8_t item);
	uint32_t addData(uint8_t item);
	uint32_t allocBSS(size_t size);

	uint32_t getTextSize() const		{ return text_segment.size(); }
	uint32_t getDataSize() const		{ return data_segment.size(); }
	uint32_t getBssSize() const			{ return file_header.a_bss; }
	uint32_t getEntryPoint() const		{ return file_header.a_entry; }

	void setTextBase(uint32_t base) { textBase = base; }
	void setDataBase(uint32_t base) { dataBase = base; }
	void setBssBase(uint32_t base)	{ bssBase = base; }
	void updateBssSymbols();

	uint32_t getTextBase() const	{ return textBase; }
	uint32_t getDataBase() const	{ return dataBase; }
	uint32_t getBssBase() const		{ return bssBase; }

	uint8_t *textPtr() { return text_segment.data(); }
	uint8_t *dataPtr() { return data_segment.data(); }

	void setEntryPoint(uint16_t val) { file_header.a_entry = val; }

	// symbols
	void addSymbol(const std::string &name, SymbolEntity &sym);
	uint32_t addString(const std::string &name);
	size_t indexOfSymbol(const std::string &name);
	SymbolEntity symbolAt(size_t index);
	bool findSymbol(const std::string &name, SymbolEntity &sym);
	bool findCodeSymbolByAddr(uint16_t addr, std::string &name);
	bool findDataSymbolByAddr(uint16_t addr, std::string &name);
	bool findNearestCodeSymbolToAddr(uint16_t addr, std::string &name, uint16_t &symAddr);

	// relocations
	void addTextRelocation(RelocationEntry&);
	void addDataRelocation(RelocationEntry&);
	bool relocate(const std::vector<ObjectFile*>&);
	void concat(ObjectFile *rhs);

	uint32_t getTextRelocSize() const { return file_header.a_trsize; }
	uint32_t getDataRelocSize() const { return file_header.a_drsize; }

	// debug output
	void dumpHeader(FILE*);
	void dumpText(FILE*);
	void dumpData(FILE*);
	void dumpTextRelocs(FILE*);
	void dumpDataRelocs(FILE*);
	void dumpSymbols(FILE*);
};

// helper functions
void hexDumpGroup(FILE *f, uint8_t *buf);
void hexDumpLine(FILE *f, uint32_t offset, uint8_t *buf);
void hexDumpSegment(FILE *f, uint8_t *seg, size_t size);

#endif // __AOUT_H

