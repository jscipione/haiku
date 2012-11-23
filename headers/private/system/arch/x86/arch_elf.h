/*
** Copyright 2003, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/
#ifndef _KERNEL_ARCH_x86_ELF_H
#define _KERNEL_ARCH_x86_ELF_H

#include "arch_elf_boot.h"


#define ELF_MACHINE	EM_486
#define ELF_MACHINE_OK(x) ((x) == EM_386 || (x) == EM_486)


#ifdef _BOOT_MODE
# include "../x86_64/arch_elf_boot.h"
#endif

#endif	/* _KERNEL_ARCH_x86_ELF_H */
