/*
 * Copyright 2012, Alex Smith, alex@alex-smith.me.uk.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_x86_64_ELF_H
#define _KERNEL_ARCH_x86_64_ELF_H

#include "arch_elf_boot.h"


#define ELF_MACHINE	EM_X86_64
#define ELF_MACHINE_OK(x) ((x) == EM_X86_64)


#ifdef _BOOT_MODE
# include "../x86/arch_elf_boot.h"
#endif

#endif	/* _KERNEL_ARCH_x86_64_ELF_H */
