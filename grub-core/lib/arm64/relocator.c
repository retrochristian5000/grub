/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2024  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>;.
 *
 */
#include <grub/mm.h>
#include <grub/misc.h>

#include <grub/types.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/cache.h>

#include <grub/arm64/relocator.h>
#include <grub/relocator_private.h>

/* Defines to simplify creating machine code instructions */

#define AARCH64_INSTR_MOVZ	0xd2800000
#define AARCH64_INSTR_MOVK	0xf2800000
#define AARCH64_INSTR_BR	0xd61f0000
#define AARCH64_INSTR_NOP	0xd503201f

#define AARCH64_INSTR_SHIFT_16 1 << 21
#define AARCH64_INSTR_SHIFT_32 2 << 21
#define AARCH64_INSTR_SHIFT_48 3 << 21

extern grub_uint8_t grub_relocator_forward_start;
extern grub_uint8_t grub_relocator_forward_end;
extern grub_uint8_t grub_relocator_backward_start;
extern grub_uint8_t grub_relocator_backward_end;
extern grub_uint32_t grub_relocator_check_endian;

#define REGW_SIZEOF (4 * sizeof (grub_uint32_t))
#define JUMP_SIZEOF (2 * sizeof (grub_uint32_t))

#define RELOCATOR_SRC_SIZEOF(x) (&grub_relocator_##x##_end \
				 - &grub_relocator_##x##_start)
#define RELOCATOR_SIZEOF(x) (RELOCATOR_SRC_SIZEOF(x) \
			     + REGW_SIZEOF * 3)

grub_size_t grub_relocator_align = sizeof (grub_uint32_t);
grub_size_t grub_relocator_forward_size;
grub_size_t grub_relocator_backward_size;
grub_size_t grub_relocator_jumper_size = JUMP_SIZEOF + REGW_SIZEOF;
grub_size_t grub_relocator_preamble_size = 0;

void
grub_cpu_relocator_init (void)
{
  grub_relocator_forward_size = RELOCATOR_SIZEOF (forward);
  grub_relocator_backward_size = RELOCATOR_SIZEOF (backward);
}

static void
write_reg (int regn, grub_uint64_t val, void **target)
{
  grub_uint16_t hword;

  hword = val >> 48 & 0xFFFF;
  *(grub_uint32_t *) *target = AARCH64_INSTR_MOVZ | AARCH64_INSTR_SHIFT_48 | (hword << 5) | regn;
  *target = ((grub_uint32_t *) *target) + 1;

  hword = val >> 32 & 0xFFFF;
  *(grub_uint32_t *) *target = AARCH64_INSTR_MOVK | AARCH64_INSTR_SHIFT_32 | (hword << 5) | regn;
  *target = ((grub_uint32_t *) *target) + 1;

  hword = val >> 16 & 0xFFFF;
  *(grub_uint32_t *) *target = AARCH64_INSTR_MOVK | AARCH64_INSTR_SHIFT_16 | (hword << 5) | regn;
  *target = ((grub_uint32_t *) *target) + 1;

  hword = val >> 0 & 0xFFFF;
  *(grub_uint32_t *) *target = AARCH64_INSTR_MOVK | (hword << 5) | regn;
  *target = ((grub_uint32_t *) *target) + 1;
}

static void
write_jump (int regn, void **target)
{
  /* br x(regn) */
  *(grub_uint32_t *) *target = AARCH64_INSTR_BR | (regn << 5);
  *target = ((grub_uint32_t *) *target) + 1;

  /* nop.  */
  *(grub_uint32_t *) *target = AARCH64_INSTR_NOP;
  *target = ((grub_uint32_t *) *target) + 1;
}

void
grub_cpu_relocator_jumper (void *rels, grub_addr_t addr)
{
  write_reg (8, addr, &rels);
  write_jump (8, &rels);
}

void
grub_cpu_relocator_backward (void *ptr, void *src, void *dest,
			     grub_size_t size)
{
  write_reg (8, (grub_uint64_t) src, &ptr);
  write_reg (9, (grub_uint64_t) dest, &ptr);
  write_reg (10, (grub_uint64_t) size, &ptr);

  grub_memcpy (ptr, &grub_relocator_backward_start,
	       RELOCATOR_SRC_SIZEOF (backward));
}

void
grub_cpu_relocator_forward (void *ptr, void *src, void *dest,
			    grub_size_t size)
{
  write_reg (8, (grub_uint64_t) src, &ptr);
  write_reg (9, (grub_uint64_t) dest, &ptr);
  write_reg (10, (grub_uint64_t) size, &ptr);

  grub_memcpy (ptr, &grub_relocator_forward_start, 
	       RELOCATOR_SRC_SIZEOF (forward));
}

void
grub_cpu_relocator_preamble (void *rels __attribute__((unused)))
{
}

grub_err_t
grub_relocator64_boot (struct grub_relocator *rel,
		       struct grub_relocator64_state state)
{
  grub_relocator_chunk_t ch;
  void *ptr;
  grub_err_t err;
  void *relst;
  grub_size_t relsize;
  grub_size_t stateset_size = 32 * REGW_SIZEOF + JUMP_SIZEOF;
  unsigned i;

  err = grub_relocator_alloc_chunk_align (rel, &ch, 0,
					  (0xffffffff - stateset_size)
					  + 1, stateset_size,
					  sizeof (grub_uint32_t),
					  GRUB_RELOCATOR_PREFERENCE_NONE, 0);
  if (err)
    return err;

  ptr = get_virtual_current_address (ch);
  for (i = 0; i < 32; i++)
    write_reg (i, state.x[i], &ptr);
  write_jump (8, &ptr);

  err = grub_relocator_prepare_relocs (rel, get_physical_target_address(ch), &relst, &relsize);
  if (err)
    return err;

  grub_arch_sync_caches ((void *) relst, relsize);

  ((void (*) (void)) relst) ();

  /* Not reached.  */
  return GRUB_ERR_NONE;
}
