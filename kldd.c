/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2018 Alexander Eremin <alexander.r.eremin@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <libelf.h>
#include <sys/link.h>
#include <sys/elf.h>
#include <sys/machelf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "kldd.h"

static const char *search_dirs[] = {
	"/platform/i86pc/kernel",
	"/kernel"
};

#define	N_SEARCH_DIRS	(sizeof (search_dirs) / sizeof (search_dirs[0]))

char *prog_name;
static int files = 0;

static void
usage()
{
	(void) fprintf(stderr, gettext(
"Usage: %s file ...\n"), prog_name);
}

static void
print_depends(int class, const char *depend)
{
	int i;
	char buf[PATH_MAX];
	char *p;

	if (class == ELFCLASS64 && (p = strstr(depend, "/")) != NULL) {
		(void) strncpy(buf, depend, p - depend);
		buf[p - depend] = '\0';
		(void) sprintf(buf + (p - depend), "/amd64/%s",  p + 1);
	}

	for (i = 0; i < N_SEARCH_DIRS; i++) {
		char modpath[PATH_MAX];
		(void) sprintf(modpath, "%s/%s", search_dirs[i],
		    class ==  ELFCLASS64 ? buf : depend);
		if (access(modpath, F_OK) == 0) {
			(void) printf("\t%s =>\t%s\n", depend, modpath);
		}
	}
}

static void
kldd_dynamic(Elf *elf_file, SCNTAB *p_scns, int num_scns, char *filename)
{
	Elf_Data	*dyn_data;
	GElf_Dyn	p_dyn;
	GElf_Ehdr	p_ehdr;
	const char	*depend;
	char unixpath[PATH_MAX];

	(void) gelf_getehdr(elf_file, &p_ehdr);

	if (p_ehdr.e_type != ET_REL) {
		(void) fprintf(stderr, "%s: not a kernel module, use ldd(1)\n",
		    filename);
		return;
	}

	for (; num_scns > 0; num_scns--, p_scns++) {
		GElf_Word	link;
		int		i;

		if (p_scns->p_shdr.sh_type != SHT_DYNAMIC)
			continue;

		if ((dyn_data = elf_getdata(p_scns->p_sd, NULL)) == 0) {
			(void) fprintf(stderr, "%s: %s: no data in "
			    "%s section\n", prog_name, filename,
			    p_scns->scn_name);
			return;
		}

		link = p_scns->p_shdr.sh_link;
		i = 0;

		(void) gelf_getdyn(dyn_data, i++, &p_dyn);
		while (p_dyn.d_tag == DT_NEEDED) {
				depend = (char *)elf_strptr(elf_file, link,
					    p_dyn.d_un.d_ptr);
				if (depend && *depend) {
				(void) print_depends(p_ehdr.e_ident[EI_CLASS],
				    depend);
			}
			(void) gelf_getdyn(dyn_data, i++, &p_dyn);
		}
	}

	(void) sprintf(unixpath, "%s/%s", search_dirs[0],
	    p_ehdr.e_ident[EI_CLASS] ==  ELFCLASS64 ? "amd64/unix" : "unix");
	if (access(unixpath, F_OK) == 0)
		(void) printf("\tunix (parent) =>\t%s\n", unixpath);

	(void) sprintf(unixpath, "%s/%s", search_dirs[1],
	    p_ehdr.e_ident[EI_CLASS] ==  ELFCLASS64 ? "amd64/genunix"
	    : "genunix");
	if (access(unixpath, F_OK) == 0)
		(void) printf("\tgenunix (parent dependency) =>\t%s\n",
		    unixpath);
}

static void
kldd_section_table(Elf *elf_file, char *filename)
{
	static SCNTAB	*sc_buf, *p_scns;
	Elf_Scn		*scn = 0;
	unsigned int	num_scns;
	size_t		shstrndx;
	size_t		shnum;

	if (elf_getshdrnum(elf_file, &shnum) == -1) {
		(void) fprintf(stderr,
		    "%s: %s: elf_getshdrnum failed: %s\n",
		    prog_name, filename, elf_errmsg(-1));
		return;
	}
	if (elf_getshdrstrndx(elf_file, &shstrndx) == -1) {
		(void) fprintf(stderr,
		    "%s: %s: elf_getshdrstrndx failed: %s\n",
		    prog_name, filename, elf_errmsg(-1));
		return;
	}

	if ((sc_buf = calloc(shnum, sizeof (SCNTAB))) == NULL) {
		(void) fprintf(stderr, "%s: %s: cannot calloc space\n",
		    prog_name, filename);
		return;
	}
	num_scns = (int)shnum - 1;
	p_scns = sc_buf;

	while ((scn = elf_nextscn(elf_file, scn)) != 0) {
		if ((gelf_getshdr(scn, &sc_buf->p_shdr) != NULL) &&
		    (sc_buf->p_shdr.sh_type == SHT_DYNAMIC)) {
		sc_buf->p_sd   =  scn;
		sc_buf++;
		}
	}

	kldd_dynamic(elf_file, p_scns, num_scns, filename);
}

static void
check_elf(char *filename)
{
	Elf *elf_file;
	int fd;
	Elf_Kind   file_type;
	struct stat buf;

	Elf_Cmd ecmd;
	errno = 0;

	if (stat(filename, &buf) == -1) {
		int	err = errno;
		(void) fprintf(stderr, "%s: %s: %s\n", prog_name, filename,
		    strerror(err));
		return;
	}

	if ((fd = open((filename), O_RDONLY)) == -1) {
		(void) fprintf(stderr, "%s: %s: cannot read\n", prog_name,
		    filename);
		return;
	}
	ecmd = ELF_C_READ;
	if ((elf_file = elf_begin(fd, ecmd, (Elf *)0)) == NULL) {
		(void) fprintf(stderr, "%s: %s: %s\n", prog_name, filename,
		    elf_errmsg(-1));
		return;
	}

	file_type = elf_kind(elf_file);
		if (file_type == ELF_K_ELF) {
			if (files)
				(void) printf("%s:\n", filename);
		kldd_section_table(elf_file, filename);
	} else {
		(void) fprintf(stderr, "%s: %s: invalid file type\n",
			    prog_name, filename);
	}
	(void) elf_end(elf_file);
	(void) close(fd);
}

int
main(int argc, char *argv[])
{
	prog_name = argv[0];
	(void) setlocale(LC_ALL, "");

	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, "%s: libelf is out of date\n",
		    prog_name);
		exit(1);
	}

	if (argc < 2) {
		usage();
		exit(1);
	} else if (argc > 2) {
		files++;
	}

	while (optind < argc) {
		check_elf(argv[optind]);
		optind++;
	}
	return (0);
}
