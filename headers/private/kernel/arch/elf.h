/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_ARCH_ELF_H
#define _KERNEL_ARCH_ELF_H


#include <elf_common.h>


struct elf_image_info;


#ifdef __cplusplus
extern "C" {
#endif

// Score an ELF binary ident. Arguments correspond to the matching FatELF
// fields.
//
// Preference is given to binaries with a higher score. Binaries with a score
// of 0 will be considered unsupported.
extern uint32_t arch_elf_score_abi_ident(uint16_t machine, uint8_t osabi, 
	uint8_t osabi_version, uint8_t word_size, uint8_t byte_order);

extern int arch_elf_relocate_rel(struct elf_image_info *image,
	struct elf_image_info *resolve_image, elf_rel *rel, int rel_len);
extern int arch_elf_relocate_rela(struct elf_image_info *image,
	struct elf_image_info *resolve_image, elf_rela *rel, int rel_len);

#ifdef __cplusplus
}
#endif


#include <arch_elf.h>


#endif	/* _KERNEL_ARCH_ELF_H */
