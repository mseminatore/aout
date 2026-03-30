#include "../aout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_dump_header    = 0;
static int g_dump_text      = 0;
static int g_dump_data      = 0;
static int g_dump_relocs    = 0;
static int g_dump_symbols   = 0;

static void usage(void)
{
	puts("\nusage: dump [options] filename\n");
	puts("-h\tdump header (default)");
	puts("-t\tdump text segment");
	puts("-d\tdump data segment");
	puts("-r\tdump relocations");
	puts("-s\tdump symbols");
	puts("-a\tdump all\n");
	exit(0);
}

/* Returns index of first non-option argument */
static int parse_opts(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
	{
		switch (argv[i][1])
		{
		case 'h': g_dump_header  = 1; break;
		case 't': g_dump_text    = 1; break;
		case 'd': g_dump_data    = 1; break;
		case 'r': g_dump_relocs  = 1; break;
		case 's': g_dump_symbols = 1; break;
		case 'a':
			g_dump_header = g_dump_text = g_dump_data = 1;
			g_dump_relocs = g_dump_symbols = 1;
			break;
		default:
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			usage();
		}
	}
	return i;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		usage();

	int first_file = parse_opts(argc, argv);

	if (first_file >= argc)
	{
		fputs("error: no input file specified\n", stderr);
		return 1;
	}

	/* Default: show header when no segment flags given */
	if (!g_dump_header && !g_dump_text && !g_dump_data &&
	    !g_dump_relocs && !g_dump_symbols)
		g_dump_header = 1;

	aout_object_file_t *obj = aout_create();
	if (!obj)
	{
		fputs("error: out of memory\n", stderr);
		return 1;
	}

	const char *filename = argv[first_file];
	if (aout_read_file_named(obj, filename) != AOUT_OK)
	{
		fprintf(stderr, "error: cannot read file '%s'\n", filename);
		aout_free(obj);
		return 1;
	}

	if (!aout_is_valid(obj))
	{
		fprintf(stderr, "error: '%s' is not a valid a.out file\n", filename);
		aout_free(obj);
		return 1;
	}

	printf("\nDump of file: %s\n\n", filename);

	/* A file with no relocation entries is a stripped executable */
	if (aout_get_text_reloc_size(obj) == 0 && aout_get_data_reloc_size(obj) == 0)
		puts("File Type: STRIPPED EXECUTABLE\n");
	else
		puts("File Type: OBJECT FILE\n");

	if (g_dump_header)
		aout_dump_header(obj, stdout);

	if (g_dump_text)
		aout_dump_text(obj, stdout);

	if (g_dump_data)
		aout_dump_data(obj, stdout);

	if (g_dump_relocs)
	{
		aout_dump_text_relocs(obj, stdout);
		aout_dump_data_relocs(obj, stdout);
	}

	if (g_dump_symbols)
		aout_dump_symbols(obj, stdout);

	aout_free(obj);
	return 0;
}
