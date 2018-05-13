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

#include	<sys/elf.h>
#include	<sys/machelf.h>
#include	<gelf.h>

typedef struct scntab {
	char		*scn_name;
	Elf_Scn		*p_sd;
	GElf_Shdr	p_shdr;
} SCNTAB;

