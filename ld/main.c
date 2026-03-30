#include "../aout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_output  = "a.out";
static uint32_t    g_base    = 0;

static void usage(void)
{
	puts("\nusage: ld [options] file1.o [file2.o ...]\n");
	puts("-o outfile\tset output filename (default: a.out)");
	puts("-b addr\t\tset text segment base address (default: 0)\n");
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
		case 'o':
			if (i + 1 >= argc)
			{
				fputs("error: -o requires a filename\n", stderr);
				exit(1);
			}
			g_output = argv[++i];
			break;
		case 'b':
			if (i + 1 >= argc)
			{
				fputs("error: -b requires an address\n", stderr);
				exit(1);
			}
			g_base = (uint32_t)strtoul(argv[++i], NULL, 0);
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
		fputs("error: no input files specified\n", stderr);
		return 1;
	}

	puts("Linking...");

	/* Load the first object file as the base result */
	aout_object_file_t *result = aout_create();
	if (!result)
	{
		fputs("error: out of memory\n", stderr);
		return 1;
	}

	printf("  %s\n", argv[first_file]);
	if (aout_read_file_named(result, argv[first_file]) != AOUT_OK)
	{
		fprintf(stderr, "error: cannot read '%s'\n", argv[first_file]);
		aout_free(result);
		return 1;
	}

	if (!aout_is_valid(result))
	{
		fprintf(stderr, "error: '%s' is not a valid a.out file\n", argv[first_file]);
		aout_free(result);
		return 1;
	}

	/* Concatenate remaining object files left-to-right */
	int i;
	for (i = first_file + 1; i < argc; i++)
	{
		aout_object_file_t *rhs = aout_create();
		if (!rhs)
		{
			fputs("error: out of memory\n", stderr);
			aout_free(result);
			return 1;
		}

		printf("  %s\n", argv[i]);
		if (aout_read_file_named(rhs, argv[i]) != AOUT_OK)
		{
			fprintf(stderr, "error: cannot read '%s'\n", argv[i]);
			aout_free(rhs);
			aout_free(result);
			return 1;
		}

		if (!aout_is_valid(rhs))
		{
			fprintf(stderr, "error: '%s' is not a valid a.out file\n", argv[i]);
			aout_free(rhs);
			aout_free(result);
			return 1;
		}

		/* Set rhs bases so concat adjusts symbol values to merged offsets */
		aout_set_text_base(rhs, aout_get_text_size(result));
		aout_set_data_base(rhs, aout_get_data_size(result));
		aout_set_bss_base(rhs,  aout_get_bss_size(result));

		aout_concat(result, rhs);
		aout_free(rhs);
	}

	/* Set segment base addresses */
	uint32_t text_size = aout_get_text_size(result);
	uint32_t data_size = aout_get_data_size(result);

	aout_set_text_base(result, g_base);
	aout_set_data_base(result, g_base + text_size);
	aout_set_bss_base(result,  g_base + text_size + data_size);
	aout_update_bss_symbols(result);

	/* Resolve relocations — all symbols now live in the merged result */
	int rc = aout_relocate(result, &result, 1);
	if (rc != AOUT_OK)
	{
		if (rc == AOUT_ERROR_SYMBOL_NOT_FOUND)
			fputs("error: unresolved external symbol(s)\n", stderr);
		else
			fprintf(stderr, "error: relocation failed (%d)\n", rc);
		aout_free(result);
		return 1;
	}

	if (aout_write_file_named(result, g_output) != AOUT_OK)
	{
		fprintf(stderr, "error: cannot write '%s'\n", g_output);
		aout_free(result);
		return 1;
	}

	printf("\nLinking complete -> %s\n\n", g_output);

	aout_free(result);
	return 0;
}
