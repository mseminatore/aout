#include "../aout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_strip_relocs    = 0;
static int g_strip_all       = 0;
static const char *g_output  = "a.out";

static void usage(void)
{
	puts("\nusage: strip [options] filename\n");
	puts("-r\t\tstrip relocation entries");
	puts("-a\t\tstrip all (symbols + relocations)");
	puts("-o file\t\tset output filename (default: a.out)\n");
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
		case 'r': g_strip_relocs = 1; break;
		case 'a': g_strip_all    = 1; break;
		case 'o':
			if (i + 1 >= argc)
			{
				fputs("error: -o requires a filename\n", stderr);
				exit(1);
			}
			g_output = argv[++i];
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

	/* Always strip symbols */
	aout_strip_symbols(obj);

	if (g_strip_relocs || g_strip_all)
		aout_strip_relocations(obj);

	if (aout_write_file_named(obj, g_output) != AOUT_OK)
	{
		fprintf(stderr, "error: cannot write '%s'\n", g_output);
		aout_free(obj);
		return 1;
	}

	printf("\nStrip complete -> %s\n\n", g_output);

	aout_free(obj);
	return 0;
}
