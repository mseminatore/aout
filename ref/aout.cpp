#include "aout.h"
#include <assert.h>
#include <ctype.h>

//
ObjectFile::ObjectFile()
{
	clear();
}

//
ObjectFile::~ObjectFile()
{

}

// reset the state of this object file
void ObjectFile::clear()
{
	// init file header properties
	memset(&file_header, 0, sizeof(file_header));

	// default to NMAGIC format?
	file_header.a_magic = 263;

	text_segment.clear();
	data_segment.clear();

	textRelocs.clear();
	dataRelocs.clear();

	symbolTable.clear();
	symbolLookup.clear();
	codeSymbolRLookup.clear();
	dataSymbolRLookup.clear();
	bssSymbolRLookup.clear();

	stringTable.clear();

	textBase = dataBase = bssBase = 0;
}

// return the symbol at the given index
SymbolEntity ObjectFile::symbolAt(size_t index)
{
	return symbolTable[index].second;
}

// find the symbol by name
bool ObjectFile::findSymbol(const std::string &name, SymbolEntity &sym)
{
	auto index = indexOfSymbol(name);
	if (index != UINT_MAX)
	{
		sym = symbolAt(index);
		return true;
	}

	return false;
}

// find a code symbol by address
bool ObjectFile::findCodeSymbolByAddr(uint16_t addr, std::string &name)
{
	name = "<none>";
	auto it = codeSymbolRLookup.find(addr);
	if (it != codeSymbolRLookup.end())
	{
		name = it->second;
		return true;
	}

	return false;
}

// find a data symbol by address
bool ObjectFile::findDataSymbolByAddr(uint16_t addr, std::string &name)
{
	name = "<none>";
	auto it = dataSymbolRLookup.find(addr);
	if (it != dataSymbolRLookup.end())
	{
		name = it->second;
		return true;
	}

	it = bssSymbolRLookup.find(addr);
	if (it != bssSymbolRLookup.end())
	{
		name = it->second;
		return true;
	}

	return false;
}

// find nearst code symbol less than given address
bool ObjectFile::findNearestCodeSymbolToAddr(uint16_t addr, std::string &name, uint16_t &symAddr)
{
	auto it = codeSymbolRLookup.lower_bound(addr);
	if (it == codeSymbolRLookup.end())
		return false;

	if (it->first != addr && it != codeSymbolRLookup.begin())
		it--;

	symAddr = (uint16_t)it->first;
	name = it->second;

	return true;
}

//
void ObjectFile::updateBssSymbols()
{
	// Note: we must update the symbolTable addresses so that BSS 
	for (auto it = symbolTable.begin(); it != symbolTable.end(); it++)
	{
		if (it->second.type & SET_BSS)
			it->second.value += getBssBase();
	}
}

//
void ObjectFile::concat(ObjectFile *rhs)
{
	// combine headers
	file_header.a_bss		+= rhs->file_header.a_bss;
	file_header.a_data		+= rhs->file_header.a_data;
	file_header.a_text		+= rhs->file_header.a_text;
	file_header.a_drsize	+= rhs->file_header.a_drsize;
	file_header.a_trsize	+= rhs->file_header.a_trsize;

	// merge contents of text and data segments
	text_segment.insert(text_segment.end(), rhs->text_segment.begin(), rhs->text_segment.end());
	data_segment.insert(data_segment.end(), rhs->data_segment.begin(), rhs->data_segment.end());

	// Note: the bss is all zero so no merging of contents is required

	// merge the symbols
	for (auto it = rhs->symbolTable.begin(); it != rhs->symbolTable.end(); it++)
	{
		// see if the rhs symbol exists in this module
		SymbolEntity se;
		if (findSymbol(it->first, se) && (se.type & SET_UNDEFINED))
		{
			auto index = indexOfSymbol(it->first);
			assert(index != UINT_MAX);

			auto sym = &symbolTable[index].second;

			// if it does and it is defined in rhs module, then update it
			sym->type = it->second.type;

			if (sym->type & SET_TEXT)
				sym->value = it->second.value + rhs->getTextBase();
			else if (sym->type & SET_DATA)
				sym->value = it->second.value + rhs->getDataBase();
			else if (sym->type & SET_BSS)
				sym->value = it->second.value + rhs->getBssBase();
		}
		else
		{
			// if not and it is defined in rhs module, then add it
			if (!(it->second.type & SET_UNDEFINED))
			{
				auto sym = it->second;
				if (it->second.type & SET_TEXT)
					sym.value = it->second.value + rhs->getTextBase();
				else if (it->second.type & SET_DATA)
					sym.value = it->second.value + rhs->getDataBase();
				else if (it->second.type & SET_BSS)
					sym.value = it->second.value + rhs->getBssBase();
				else
					assert(false);

				addSymbol(it->first, sym);
			}
		}
	}

	// fixup locations in the lhs module
	for (auto it = textRelocs.begin(); it != textRelocs.end(); it++)
	{
		if (it->external)
		{
			// look up the symbol
			auto sym = &symbolTable[it->index].second;

			// if the symbol has been resolved for this module, fixup the address, index and external flags
			if (!(sym->type & SET_EXTERN))
			{
				it->address = sym->value;
				it->external = 0;

				if (sym->type & SET_TEXT)
					it->index = SEG_TEXT;
				else if (sym->type & SET_DATA)
					it->index = SEG_DATA;
				else if (sym->type & SET_BSS)
					it->index = SEG_BSS;
				else
					assert(false);
			}
		}
	}

	// merge relocations updating addresses
	for (auto it = rhs->textRelocs.begin(); it != rhs->textRelocs.end(); it++)
	{
		if (it->external)
			continue;

		auto re = *it;

		if (it->index == SEG_TEXT)
			re.address += rhs->getTextBase();
		else if (it->index == SEG_DATA)
			re.address += rhs->getDataBase();
		else if (it->index == SEG_BSS)
			re.address += rhs->getBssBase();
		else
			assert(false);

		textRelocs.push_back(re);
	}
}

//
bool ObjectFile::relocate(const std::vector<ObjectFile*> &modules)
{
	// for each code segment relocation
	for (auto it = textRelocs.begin(); it != textRelocs.end(); it++)
	{
		int iSymbolFound = 0;

		if (it->external)
		{
			auto name = symbolTable[it->index].first;
			auto sym = symbolTable[it->index].second;
			assert(sym.type & SET_UNDEFINED);

			// find address of external symbol and patch it into this segment
			SymbolEntity externSym;
			for (auto moduleIter = modules.begin(); moduleIter != modules.end(); moduleIter++)
			{
				// look for the symbol in the module
				auto module = (*moduleIter);
				if (module->findSymbol(name, externSym))
				{
					// see if it is defined in this module
					if (!(externSym.type & SET_UNDEFINED))
					{
						// it is defined in this module
						if (externSym.type & SET_TEXT)
						{
							auto addr = module->getTextBase() + externSym.value;
							text_segment[it->address] = LOBYTE(addr);
							text_segment[it->address + 1] = HIBYTE(addr);
							iSymbolFound++;
						}
						else if (externSym.type & SET_DATA)
						{
							auto addr = module->getDataBase() + externSym.value;
							text_segment[it->address] = LOBYTE(addr);
							text_segment[it->address + 1] = HIBYTE(addr);
							iSymbolFound++;
						}
						else if (externSym.type & SET_BSS)
						{
							auto addr = module->getBssBase() + externSym.value;
							text_segment[it->address] = LOBYTE(addr);
							text_segment[it->address + 1] = HIBYTE(addr);
							iSymbolFound++;
						}
						else
						{
							assert(false);
						}
					}
				}
			}

			if (0 == iSymbolFound)
			{
				fprintf(stderr, "Error: Symbol '%s' not found!\n", name.c_str());
				return false;
			}
			else if (iSymbolFound > 1)
			{
				fprintf(stderr, "Error: Symbol '%s' multiply defined!\n", name.c_str());
				return false;
			}
		}
		else
		{
			// address is in this segment, calculate absolute addr and patch it in
			uint32_t addr = 0;
			if (it->index == SEG_TEXT)
			{
				addr = textBase + text_segment[it->address] + (text_segment[it->address + 1] << 8);
			}
			else if (it->index == SEG_DATA)
			{
				addr = dataBase + text_segment[it->address] + (text_segment[it->address + 1] << 8);
			}
			else if (it->index == SEG_BSS)
			{
				addr = bssBase + text_segment[it->address] + (text_segment[it->address + 1] << 8);
			}

			// patch the address in code segment
			text_segment[it->address] = LOBYTE(addr);
			text_segment[it->address + 1] = HIBYTE(addr);
		}
	}
	
	return true;
}

// write out the object file to the named file
int ObjectFile::writeFile(const std::string &name)
{
	FILE *f = fopen(name.c_str(), "wb");
	if (nullptr == f)
		return EOF;

	auto result = writeFile(f);
	fclose(f);

	filename = name;

	return result;
}

// write out the object file to the given stream
int ObjectFile::writeFile(FILE *fptr)
{
	assert(fptr != nullptr);
	if (fptr == nullptr)
		return EOF;

	// update the header
	file_header.a_text		= text_segment.size();
	file_header.a_data		= data_segment.size();
	file_header.a_trsize	= textRelocs.size();
	file_header.a_drsize	= dataRelocs.size();
	file_header.a_syms		= symbolTable.size();

	// write the header
	auto result = fwrite(&file_header, sizeof(file_header), 1, fptr);

	// write the text segment
	fwrite(text_segment.data(), text_segment.size(), 1, fptr);

	// write the data segment
	fwrite(data_segment.data(), data_segment.size(), 1, fptr);

	// write the text relocations
	for (auto iter = textRelocs.begin(); iter != textRelocs.end(); iter++)
	{
		fwrite(&(*iter), sizeof(RelocationEntry), 1, fptr);
	}

	// write the data relocations
	for (auto iter = dataRelocs.begin(); iter != dataRelocs.end(); iter++)
	{
		fwrite(&(*iter), sizeof(RelocationEntry), 1, fptr);
	}

	// write the symbol table
	for (auto iter = symbolTable.begin(); iter != symbolTable.end(); iter++)
	{
		auto se = (*iter).second;
		fwrite(&se, sizeof(se), 1, fptr);
	}

	// write the string table
	for (size_t i = 0; i < stringTable.size(); i++)
	{
		fputc(stringTable[i], fptr);
	}

	return 0;
}

// read in the named object file
int ObjectFile::readFile(const std::string &name)
{
	FILE *f = fopen(name.c_str(), "rb");
	if (nullptr == f)
		return EOF;

	auto result = readFile(f);
	if (result)
		return EOF;

	fclose(f);

	filename = name;

	return result;
}

// read an object file from the given stream
int ObjectFile::readFile(FILE *fptr)
{
	assert(fptr != nullptr);
	if (fptr == nullptr)
		return EOF;

	clear();

	// read the header
	auto result = fread(&file_header, sizeof(file_header), 1, fptr);
	if (result != 1)
		return EOF;

	// read the text segment
	for (int i = 0; i < file_header.a_text; i++)
	{
		auto c = fgetc(fptr);
		text_segment.push_back(c);
	}

	// read the data segment
	for (int i = 0; i < file_header.a_data; i++)
	{
		auto c = fgetc(fptr);
		data_segment.push_back(c);
	}

	// read the text relocations
	for (int i = 0; i < file_header.a_trsize; i++)
	{
		RelocationEntry re;

		fread(&re, sizeof(re), 1, fptr);
		textRelocs.push_back(re);
	}

	// read the data relocations
	for (int i = 0; i < file_header.a_drsize; i++)
	{
		RelocationEntry re;

		fread(&re, sizeof(re), 1, fptr);
		dataRelocs.push_back(re);
	}

	// read the symbol table
	std::vector<SymbolEntity> st;
	for (int i = 0; i < file_header.a_syms; i++)
	{
		SymbolEntity se;
		fread(&se, sizeof(se), 1, fptr);
		st.push_back(se);
	}

	// read the string table
	char c = fgetc(fptr);
	while (c != EOF)
	{
		stringTable.push_back(c);
		c = fgetc(fptr);
	}

	// read the symbols
	for (int i = 0; i < file_header.a_syms; i++)
	{
		auto sym = st[i];
		char *pStr = &(stringTable[sym.nameOffset]);
		symbolTable.push_back(SymbolTable::value_type(pStr, sym));

		auto r1 = symbolLookup.insert(SymbolLookup::value_type(pStr, i));
		assert(r1.second);

		if (sym.type & SET_TEXT)
		{
			auto result = codeSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, pStr));
			assert(result.second);
		}
		else if (sym.type & SET_DATA)
		{
			auto result = dataSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, pStr));
			assert(result.second);
		}
		else if (sym.type & SET_BSS)
		{
			auto result = bssSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, pStr));
			assert(result.second);
		}
	}

	return 0;
}

// alloc space in bss segment and return its address
uint32_t ObjectFile::allocBSS(size_t size)
{
	uint32_t addr = file_header.a_bss;
	file_header.a_bss += size;

	return addr;
}

// alloc/assign byte to code segment and return its address
uint32_t ObjectFile::addText(uint8_t item)
{
	uint32_t addr = text_segment.size();
		text_segment.push_back(item);
	return addr;
}

// alloc/assign data byte to data segment and return its address
uint32_t ObjectFile::addData(uint8_t item)
{
	uint32_t addr = data_segment.size();
		data_segment.push_back(item);
	return addr;
}

//
void ObjectFile::addSymbol(const std::string &name, SymbolEntity &sym)
{
	auto it = symbolLookup.find(name);

	// if symbol already exists as an EXTERN, update it since it is now defined
	if (it != symbolLookup.end())
	{
		symbolTable[it->second].second.type = sym.type;
		symbolTable[it->second].second.value = sym.value;
		return;
	}

	uint32_t offset = addString(name);
	sym.nameOffset = offset;

	auto index = symbolTable.size();

	symbolTable.push_back(SymbolTable::value_type(name, sym));
	auto sl = symbolLookup.insert(SymbolLookup::value_type(name, index));
	assert(sl.second);

	if (sym.type & SET_TEXT)
	{
		auto r = codeSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, name));
		assert(r.second);
	}
	else if (sym.type & SET_DATA)
	{
		auto r = dataSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, name));
		assert(r.second);
	}
	else if (sym.type & SET_BSS)
	{
		auto r = bssSymbolRLookup.insert(SymbolRLookup::value_type(sym.value, name));
		assert(r.second);
	}

	//	assert(false);
}

//
uint32_t ObjectFile::addString(const std::string &name)
{
	uint32_t offset = stringTable.size();

	for (size_t i = 0; i < name.size(); i++)
		stringTable.push_back(name[i]);

	// make it asciiz
	stringTable.push_back(0);

	return offset;
}

//
size_t ObjectFile::indexOfSymbol(const std::string &name)
{
	auto iter = symbolLookup.find(name);

	if (iter == symbolLookup.end())
		return UINT_MAX;

	return iter->second;
}

//
void ObjectFile::addTextRelocation(RelocationEntry &r)
{
	textRelocs.push_back(r);
}

//
void ObjectFile::addDataRelocation(RelocationEntry &r)
{
	dataRelocs.push_back(r);
}

//
void ObjectFile::dumpHeader(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	fprintf(f, "A.out File Header\n");
	fprintf(f, "-----------------\n\n");

	fprintf(f, "     Magic Number: " HEX_PREFIX "%X\n", file_header.a_magic);
	fprintf(f, "Text Segment size: " HEX_PREFIX "%04X (%d) bytes\n", file_header.a_text, file_header.a_text);
	fprintf(f, "Data Segment size: " HEX_PREFIX "%04X (%d) bytes\n", file_header.a_data, file_header.a_data);
	fprintf(f, " BSS Segment size: " HEX_PREFIX "%04X (%d) bytes\n", file_header.a_bss, file_header.a_bss);
	fprintf(f, "Symbol Table size: " HEX_PREFIX "%04X (%d) bytes\n", file_header.a_syms, file_header.a_syms);
	fprintf(f, " Text reloc count: " HEX_PREFIX "%04X (%d) entries\n", file_header.a_trsize, file_header.a_trsize);
	fprintf(f, " Data reloc count: " HEX_PREFIX "%04X (%d) entries\n", file_header.a_drsize, file_header.a_drsize);
	fprintf(f, " Main Entry Point: " HEX_PREFIX "%04X\n\n", file_header.a_entry);
}

//
void hexDumpGroup(FILE *f, uint8_t *buf)
{
	for (int item = 0; item < 4; item++)
	{
		fprintf(f, "%02X ", buf[item]);
	}
}

//
void hexDumpLine(FILE *f, uint32_t offset, uint8_t *buf)
{
	fprintf(f, "%04X: ", offset);

	hexDumpGroup(f, buf);
	fputc(' ', f);
	hexDumpGroup(f, buf+4);

	fputs("- ", f);

	hexDumpGroup(f, buf+8);
	fputc(' ', f);
	hexDumpGroup(f, buf+12);

	fputc('[', f);

	// write out character row
	for (int i = 0; i < 16; i++)
	{
		if (isprint(buf[i]))
			fputc(buf[i], f);
		else
			fputc('.', f);
	}
	
	fprintf(f, "]\n");
}

//
void hexDumpSegment(FILE *f, uint8_t *seg, size_t size)
{
	uint32_t offset = 0;

	for (size_t i = 0; i < size / 16; i++)
	{
		hexDumpLine(f, offset, &seg[offset]);
		offset += 16;
	}

	// if we finished on a full line then we are done!
	if (0 == (size % 16))
		return;

	// otherwise we need to print the last (partial) line
	uint8_t lastLine[16] = { 0 };
	memcpy(lastLine, &seg[offset], size % 16);
	hexDumpLine(f, offset, lastLine);
}

//
void ObjectFile::dumpText(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	fprintf(f, ".text segment (hex)\n");
	fprintf(f, "-------------------\n\n");

	Segment::value_type *pData = text_segment.data();
	hexDumpSegment(f, pData, text_segment.size());
	fputc('\n', f);
}

//
void ObjectFile::dumpData(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	fprintf(f, ".data segment (hex)\n");
	fprintf(f, "-------------------\n\n");

	Segment::value_type *pData = data_segment.data();
	hexDumpSegment(f, data_segment.data(), data_segment.size());
	fputc('\n', f);
}

//
const char *getSegmentName(uint32_t seg)
{
	if (seg == SEG_TEXT)
		return ".text";
	else if (seg == SEG_DATA)
		return ".data";
	else if (seg == SEG_BSS)
		return ".bss";

	assert(false);
	return "unknown!";
}

// output the text relocations
void ObjectFile::dumpTextRelocs(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	if (0 == textRelocs.size())
		return;

	fprintf(f, ".text segment relocations\n");
	fprintf(f, "-------------------------\n\n");

	auto iter = textRelocs.begin();
	for (;iter != textRelocs.end(); iter++)
	{
		auto re = *iter;
		if (re.external)
			fprintf(f, "%04X\tsize: %d (bytes)\tExternal\t%s\n", re.address, 1 << re.length, symbolTable[re.index].first.c_str());
		else
			fprintf(f, "%04X\tsize: %d (bytes)\tSegment: %s\n", re.address, 1 << re.length, getSegmentName(re.index));
	}

	fputc('\n', f);
}

// output the data relocations
void ObjectFile::dumpDataRelocs(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	if (0 == dataRelocs.size())
		return;

	fprintf(f, ".data segment relocations\n");
	fprintf(f, "-------------------------\n\n");

	auto iter = dataRelocs.begin();
	for (; iter != dataRelocs.end(); iter++)
	{
		auto re = *iter;
		fprintf(f, "%04X\texternal: %d\tsize: %d (bytes)\n", re.address, re.external, 1 << re.length);
	}

	fputc('\n', f);
}

// output all of the symbol names
void ObjectFile::dumpSymbols(FILE *f)
{
	assert(f != nullptr);
	if (f == nullptr)
		return;

	if (0 == symbolTable.size())
		return;

	fprintf(f, "Symbols\n");
	fprintf(f, "-------\n\n");

	auto iter = symbolTable.begin();
	for (; iter != symbolTable.end(); iter++)
	{
		auto sym = *iter;
		std::string type;

		if (sym.second.type & SET_EXTERN)
			type += " public";
		if (sym.second.type & SET_TEXT)
			type += " .text";
		if (sym.second.type & SET_DATA)
			type += " .data";
		if (sym.second.type & SET_BSS)
			type += " .bss";
		if (sym.second.type & SET_UNDEFINED)
			type += " external";

		fprintf(f, "%15s\t%s segment\toffset: %d (" HEX_PREFIX "%04X)\n", sym.first.c_str(), type.c_str(), sym.second.value, sym.second.value);
	}
}
