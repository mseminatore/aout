#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#if !defined(_WIN32)
#  include <unistd.h>  /* close() for mkstemp fd */
#endif

/* Unix ar file format constants */
#define AR_MAGIC        "!<arch>\n"
#define AR_MAGIC_LEN    8
#define AR_FMAG         "`\n"
#define AR_FMAG_LEN     2
#define AR_HDR_SIZE     60   /* sizeof(ar_header_t) */

/* Maximum member name length that fits in the name field with '/' terminator */
#define AR_NAME_MAX     15

typedef struct ar_header_t
{
	char name[16];   /* member name, '/' terminated, space padded  */
	char date[12];   /* mtime decimal string, space padded          */
	char uid[6];     /* uid decimal string, space padded            */
	char gid[6];     /* gid decimal string, space padded            */
	char mode[8];    /* file mode octal string, space padded        */
	char size[10];   /* data size decimal string, space padded      */
	char fmag[2];    /* always "`\n"                                */
} ar_header_t;

typedef char ar_header_size_check[(sizeof(ar_header_t) == AR_HDR_SIZE) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Header helpers                                                       */
/* ------------------------------------------------------------------ */

/* Fill an ar_header_t for a member named 'name' with 'data_size' bytes */
static void ar_fill_header(ar_header_t *hdr, const char *name, long data_size)
{
	const char *base;
	time_t now = time(NULL);

	memset(hdr, ' ', sizeof(*hdr));
	memcpy(hdr->fmag, AR_FMAG, AR_FMAG_LEN);

	/* Use just the basename */
	base = strrchr(name, '/');
	base = base ? base + 1 : name;

	snprintf(hdr->name, sizeof(hdr->name), "%-15s/", base);
	snprintf(hdr->date, sizeof(hdr->date), "%-12ld", (long)now);
	snprintf(hdr->uid,  sizeof(hdr->uid),  "%-6d",   0);
	snprintf(hdr->gid,  sizeof(hdr->gid),  "%-6d",   0);
	snprintf(hdr->mode, sizeof(hdr->mode), "%-8o",   0100644);
	snprintf(hdr->size, sizeof(hdr->size), "%-10ld",  data_size);

	/* snprintf null-terminates; restore trailing space in fmag area */
	memcpy(hdr->fmag, AR_FMAG, AR_FMAG_LEN);
}

/* Extract member name from header (strips '/' terminator and trailing spaces) */
static void ar_get_name(const ar_header_t *hdr, char *out, size_t outsz)
{
	size_t i;
	strncpy(out, hdr->name, outsz - 1);
	out[outsz - 1] = '\0';
	/* strip '/' terminator */
	for (i = 0; i < outsz && out[i]; i++)
	{
		if (out[i] == '/')
		{
			out[i] = '\0';
			break;
		}
	}
	/* strip trailing spaces */
	i = strlen(out);
	while (i > 0 && out[i - 1] == ' ')
		out[--i] = '\0';
}

/* Parse 'size' field from header, returns -1 on error */
static long ar_get_size(const ar_header_t *hdr)
{
	char buf[11];
	strncpy(buf, hdr->size, 10);
	buf[10] = '\0';
	return strtol(buf, NULL, 10);
}

/* ------------------------------------------------------------------ */
/* Low-level archive I/O                                               */
/* ------------------------------------------------------------------ */

static int ar_write_magic(FILE *f)
{
	return fwrite(AR_MAGIC, 1, AR_MAGIC_LEN, f) == AR_MAGIC_LEN ? 0 : -1;
}

static int ar_check_magic(FILE *f)
{
	char buf[AR_MAGIC_LEN];
	if (fread(buf, 1, AR_MAGIC_LEN, f) != AR_MAGIC_LEN)
		return -1;
	return memcmp(buf, AR_MAGIC, AR_MAGIC_LEN) == 0 ? 0 : -1;
}

/* Read one header. Returns 1 on success, 0 on EOF, -1 on error. */
static int ar_read_header(FILE *f, ar_header_t *hdr)
{
	size_t n = fread(hdr, 1, AR_HDR_SIZE, f);
	if (n == 0)
		return 0;
	if (n != AR_HDR_SIZE)
		return -1;
	if (memcmp(hdr->fmag, AR_FMAG, AR_FMAG_LEN) != 0)
		return -1;
	return 1;
}

/* Copy 'size' bytes from src to dst.
 * Consumes the even-alignment padding byte from src if size is odd,
 * and writes a corresponding padding byte to dst. */
static int ar_copy_data(FILE *dst, FILE *src, long size)
{
	char buf[4096];
	long remaining = size;
	while (remaining > 0)
	{
		size_t chunk = (size_t)(remaining < (long)sizeof(buf) ? remaining : (long)sizeof(buf));
		size_t n = fread(buf, 1, chunk, src);
		if (n == 0)
			return -1;
		if (fwrite(buf, 1, n, dst) != n)
			return -1;
		remaining -= (long)n;
	}
	/* ar pads members to even file offsets */
	if (size & 1)
	{
		fgetc(src);        /* consume source padding byte  */
		fputc('\n', dst);  /* write destination padding    */
	}
	return 0;
}

/* Skip past the data of a member (including even-padding byte if needed) */
static int ar_skip(FILE *f, long size)
{
	long padded = size + (size & 1);
	return fseek(f, padded, SEEK_CUR) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Helper: load an entire file into memory. Caller frees *data.        */
/* ------------------------------------------------------------------ */

static int load_file(const char *path, uint8_t **data, long *size)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
	*size = ftell(f);
	rewind(f);
	*data = (uint8_t *)malloc((size_t)*size);
	if (!*data) { fclose(f); return -1; }
	if (fread(*data, 1, (size_t)*size, f) != (size_t)*size) { free(*data); fclose(f); return -1; }
	fclose(f);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Name matching                                                        */
/* ------------------------------------------------------------------ */

static int name_matches(const char *member_name, char **names, int nnames)
{
	int i;
	const char *base;
	if (nnames == 0)
		return 1; /* no filter → match all */
	for (i = 0; i < nnames; i++)
	{
		base = strrchr(names[i], '/');
		base = base ? base + 1 : names[i];
		if (strcmp(member_name, base) == 0)
			return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Operations                                                           */
/* ------------------------------------------------------------------ */

/*
 * ar -r archive file...
 * Create or replace members. Members not in the file list are kept as-is.
 */
static int cmd_replace(const char *archive, char **files, int nfiles, int verbose)
{
	/* Load new member data */
	uint8_t **fdata  = (uint8_t **)calloc((size_t)nfiles, sizeof(uint8_t *));
	long     *fsizes = (long     *)calloc((size_t)nfiles, sizeof(long));
	int i, rc = 0;

	if (!fdata || !fsizes)
	{
		fputs("error: out of memory\n", stderr);
		rc = 1; goto done;
	}

	for (i = 0; i < nfiles; i++)
	{
		if (load_file(files[i], &fdata[i], &fsizes[i]) != 0)
		{
			fprintf(stderr, "error: cannot read '%s': %s\n", files[i], strerror(errno));
			rc = 1; goto done;
		}
	}

	/* Write to a temp file, then rename */
	char tmpname[1024];
	snprintf(tmpname, sizeof(tmpname), "%s.XXXXXX", archive);

#if defined(_WIN32)
	if (_mktemp_s(tmpname, sizeof(tmpname)) != 0)
#else
	{
		int fd = mkstemp(tmpname);
		if (fd == -1)
		{
			fprintf(stderr, "error: cannot create temp file: %s\n", strerror(errno));
			rc = 1; goto done;
		}
		close(fd);
	}
	{
#endif
		FILE *out = fopen(tmpname, "wb");
		if (!out)
		{
			fprintf(stderr, "error: cannot write temp file: %s\n", strerror(errno));
			rc = 1; goto done;
		}
		ar_write_magic(out);

		/* Copy existing members, skipping those being replaced */
		FILE *in = fopen(archive, "rb");
		if (in && ar_check_magic(in) == 0)
		{
			ar_header_t hdr;
			int ret;
			while ((ret = ar_read_header(in, &hdr)) == 1)
			{
				char mname[32];
				long msize = ar_get_size(&hdr);
				ar_get_name(&hdr, mname, sizeof(mname));

				/* Check if this member is being replaced */
				int replace = 0;
				for (i = 0; i < nfiles; i++)
				{
					const char *base = strrchr(files[i], '/');
					base = base ? base + 1 : files[i];
					if (strcmp(mname, base) == 0)
					{
						replace = i + 1; /* 1-based */
						break;
					}
				}

				if (replace)
				{
					/* Skip old data; will be written from fdata below */
					ar_skip(in, msize);
				}
				else
				{
					/* Copy member as-is */
					fwrite(&hdr, 1, AR_HDR_SIZE, out);
					ar_copy_data(out, in, msize);
				}
			}
			fclose(in);
		}
		else if (in)
		{
			fclose(in);
		}

		/* Write new/replacement members */
		for (i = 0; i < nfiles; i++)
		{
			ar_header_t hdr;
			ar_fill_header(&hdr, files[i], fsizes[i]);
			fwrite(&hdr, 1, AR_HDR_SIZE, out);
			fwrite(fdata[i], 1, (size_t)fsizes[i], out);
			if (fsizes[i] & 1)
				fputc('\n', out);
			if (verbose)
				printf("a - %s\n", files[i]);
		}
		fclose(out);

		/* Replace archive with temp file */
		remove(archive);
		if (rename(tmpname, archive) != 0)
		{
			fprintf(stderr, "error: cannot rename temp file: %s\n", strerror(errno));
			remove(tmpname);
			rc = 1;
		}
#if !defined(_WIN32)
	}
#endif

done:
	if (fdata)
	{
		for (i = 0; i < nfiles; i++)
			free(fdata[i]);
		free(fdata);
	}
	free(fsizes);
	return rc;
}

/*
 * ar -t archive [file...]
 * List member names (all or filtered).
 */
static int cmd_list(const char *archive, char **names, int nnames, int verbose)
{
	FILE *f = fopen(archive, "rb");
	if (!f)
	{
		fprintf(stderr, "error: cannot open '%s': %s\n", archive, strerror(errno));
		return 1;
	}
	if (ar_check_magic(f) != 0)
	{
		fprintf(stderr, "error: '%s' is not an ar archive\n", archive);
		fclose(f);
		return 1;
	}

	ar_header_t hdr;
	int ret;
	while ((ret = ar_read_header(f, &hdr)) == 1)
	{
		char mname[32];
		long msize = ar_get_size(&hdr);
		ar_get_name(&hdr, mname, sizeof(mname));

		if (name_matches(mname, names, nnames))
		{
			if (verbose)
			{
				char date_buf[32];
				long mtime = strtol(hdr.date, NULL, 10);
				time_t t = (time_t)mtime;
				strftime(date_buf, sizeof(date_buf), "%b %e %H:%M %Y", localtime(&t));
				printf("%s %8ld %s %s\n", hdr.mode, msize, date_buf, mname);
			}
			else
			{
				puts(mname);
			}
		}

		ar_skip(f, msize);
	}

	fclose(f);
	return ret == -1 ? 1 : 0;
}

/*
 * ar -x archive [file...]
 * Extract members (all or filtered).
 */
static int cmd_extract(const char *archive, char **names, int nnames, int verbose)
{
	FILE *f = fopen(archive, "rb");
	if (!f)
	{
		fprintf(stderr, "error: cannot open '%s': %s\n", archive, strerror(errno));
		return 1;
	}
	if (ar_check_magic(f) != 0)
	{
		fprintf(stderr, "error: '%s' is not an ar archive\n", archive);
		fclose(f);
		return 1;
	}

	ar_header_t hdr;
	int ret;
	while ((ret = ar_read_header(f, &hdr)) == 1)
	{
		char mname[32];
		long msize = ar_get_size(&hdr);
		ar_get_name(&hdr, mname, sizeof(mname));

		if (name_matches(mname, names, nnames))
		{
			FILE *out = fopen(mname, "wb");
			if (!out)
			{
				fprintf(stderr, "error: cannot write '%s': %s\n", mname, strerror(errno));
				ar_skip(f, msize);
				continue;
			}
			/* copy data */
			char buf[4096];
			long remaining = msize;
			while (remaining > 0)
			{
				size_t chunk = (size_t)(remaining < (long)sizeof(buf) ? remaining : (long)sizeof(buf));
				size_t n = fread(buf, 1, chunk, f);
				if (n == 0) break;
				fwrite(buf, 1, n, out);
				remaining -= (long)n;
			}
			fclose(out);
			/* skip even-padding byte */
			if (msize & 1)
				fgetc(f);
			if (verbose)
				printf("x - %s\n", mname);
		}
		else
		{
			ar_skip(f, msize);
		}
	}

	fclose(f);
	return ret == -1 ? 1 : 0;
}

/*
 * ar -d archive file...
 * Delete named members.
 */
static int cmd_delete(const char *archive, char **files, int nfiles, int verbose)
{
	if (nfiles == 0)
	{
		fputs("error: -d requires at least one filename\n", stderr);
		return 1;
	}

	FILE *in = fopen(archive, "rb");
	if (!in)
	{
		fprintf(stderr, "error: cannot open '%s': %s\n", archive, strerror(errno));
		return 1;
	}
	if (ar_check_magic(in) != 0)
	{
		fprintf(stderr, "error: '%s' is not an ar archive\n", archive);
		fclose(in);
		return 1;
	}

	char tmpname[1024];
	snprintf(tmpname, sizeof(tmpname), "%s.XXXXXX", archive);

#if !defined(_WIN32)
	int fd = mkstemp(tmpname);
	if (fd == -1)
	{
		fprintf(stderr, "error: cannot create temp file: %s\n", strerror(errno));
		fclose(in);
		return 1;
	}
	close(fd);
#endif

	FILE *out = fopen(tmpname, "wb");
	if (!out)
	{
		fprintf(stderr, "error: cannot write temp file: %s\n", strerror(errno));
		fclose(in);
		return 1;
	}
	ar_write_magic(out);

	ar_header_t hdr;
	int ret;
	while ((ret = ar_read_header(in, &hdr)) == 1)
	{
		char mname[32];
		long msize = ar_get_size(&hdr);
		ar_get_name(&hdr, mname, sizeof(mname));

		if (name_matches(mname, files, nfiles))
		{
			if (verbose)
				printf("d - %s\n", mname);
			ar_skip(in, msize);
		}
		else
		{
			fwrite(&hdr, 1, AR_HDR_SIZE, out);
			ar_copy_data(out, in, msize);
		}
	}

	fclose(in);
	fclose(out);

	remove(archive);
	if (rename(tmpname, archive) != 0)
	{
		fprintf(stderr, "error: cannot rename temp file: %s\n", strerror(errno));
		remove(tmpname);
		return 1;
	}
	return ret == -1 ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

static void usage(void)
{
	puts("\nusage: ar [options] archive [file...]\n");
	puts("  -r\tcreate archive or add/replace members");
	puts("  -t\tlist member names");
	puts("  -x\textract members");
	puts("  -d\tdelete members");
	puts("  -v\tverbose output\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int opt_replace  = 0;
	int opt_list     = 0;
	int opt_extract  = 0;
	int opt_delete   = 0;
	int opt_verbose  = 0;

	if (argc < 3)
		usage();

	/* Options string: e.g. "rvt", "rx", etc. */
	const char *opts = argv[1];
	for (; *opts; opts++)
	{
		switch (*opts)
		{
		case 'r': opt_replace = 1; break;
		case 't': opt_list    = 1; break;
		case 'x': opt_extract = 1; break;
		case 'd': opt_delete  = 1; break;
		case 'v': opt_verbose = 1; break;
		default:
			fprintf(stderr, "unknown option '%c'\n", *opts);
			usage();
		}
	}

	int ops = opt_replace + opt_list + opt_extract + opt_delete;
	if (ops == 0 || ops > 1)
	{
		fputs("error: specify exactly one of -r, -t, -x, -d\n", stderr);
		return 1;
	}

	const char *archive = argv[2];
	char **files        = argv + 3;
	int   nfiles        = argc - 3;

	if (opt_replace)  return cmd_replace(archive, files, nfiles, opt_verbose);
	if (opt_list)     return cmd_list   (archive, files, nfiles, opt_verbose);
	if (opt_extract)  return cmd_extract(archive, files, nfiles, opt_verbose);
	if (opt_delete)   return cmd_delete (archive, files, nfiles, opt_verbose);

	return 0;
}
