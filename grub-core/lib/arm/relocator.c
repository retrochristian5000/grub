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

#include <grub/arm/relocator.h>
#include <grub/relocator_private.h>

/* Defines to simplify creating machine code instructions */

#define ARM_INSTR_LDR	0xe5900000
#define ARM_INSTR_BX	0xe12fff10
#define ARM_INSTR_NOP	0xe1a00000
#define ARM_INSTR_LDM	0xe8900000
#define ARM_INSTR_ADD	0xe2800000

extern grub_uint8_t grub_relocator_forward_start;
extern grub_uint8_t grub_relocator_forward_end;
extern grub_uint8_t grub_relocator_forward_regs;
extern grub_uint8_t grub_relocator_backward_start;
extern grub_uint8_t grub_relocator_backward_end;
extern grub_uint8_t grub_relocator_backward_regs;
extern grub_uint32_t grub_relocator_check_endian;

#define REGW_SIZEOF (1 * sizeof (grub_uint32_t))
#define JUMP_SIZEOF (3 * sizeof (grub_uint32_t))

#define RELOCATOR_SIZEOF(x) (&grub_relocator_##x##_end \
			     - &grub_relocator_##x##_start)

#define RELOCATOR_REGS_OFFSET(x) (&grub_relocator_##x##_regs \
				  - &grub_relocator_##x##_start)

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

void
grub_cpu_relocator_preamble (void *rels __attribute__((unused)))
{
}

static void
write_val (grub_uint32_t val, void **target)
{
  *(grub_uint32_t *) *target = val;
  *target = ((grub_uint32_t *) *target) + 1;
}

void
grub_cpu_relocator_jumper (void *rels, grub_addr_t addr)
{
  /* ldr r0, [pc, #4] */
  write_val (ARM_INSTR_LDR | (0xF << 16) | 0x4, &rels);

  /* bx r0 */
  write_val (ARM_INSTR_BX, &rels);

  /* nop */
  write_val (ARM_INSTR_NOP, &rels);

  /* immediate value */
  write_val (addr, &rels);
}

void
grub_cpu_relocator_backward (void *ptr, void *src, void *dest,
			     grub_size_t size)
{
  grub_memcpy (ptr, &grub_relocator_backward_start,
	       RELOCATOR_SIZEOF (backward));
  ptr = (grub_uint8_t*) ptr + RELOCATOR_REGS_OFFSET (backward);
  write_val ((grub_uint32_t) src, &ptr);
  write_val ((grub_uint32_t) dest, &ptr);
  write_val ((grub_uint32_t) size, &ptr);
}

void
grub_cpu_relocator_forward (void *ptr, void *src, void *dest,
			    grub_size_t size)
{
  grub_memcpy (ptr, &grub_relocator_forward_start,
	       RELOCATOR_SIZEOF (forward));
  ptr = (grub_uint8_t*) ptr + RELOCATOR_REGS_OFFSET (forward);
  write_val ((grub_uint32_t) src, &ptr);
  write_val ((grub_uint32_t) dest, &ptr);
  write_val ((grub_uint32_t) size, &ptr);
}

grub_err_t
grub_relocator32_boot (struct grub_relocator *rel,
		       struct grub_relocator32_state state)
{
  grub_relocator_chunk_t ch;
  void *ptr;
  grub_err_t err;
  void *relst;
  grub_size_t relsize;
  grub_size_t stateset_size = 4 * REGW_SIZEOF + 15 * REGW_SIZEOF;
  unsigned i;

  err = grub_relocator_alloc_chunk_align (rel, &ch, 0,
					  (0xffffffff - stateset_size)
					  + 1, stateset_size,
					  sizeof (grub_uint32_t),
					  GRUB_RELOCATOR_PREFERENCE_NONE, 0);

  if (err)
    return err;

  ptr = get_virtual_current_address (ch);

  /* add r0, pc, #8 */
  write_val (ARM_INSTR_ADD | (0xF << 16) | 0x8, &ptr);

  /* ldm r0, {r0-r14} */
  write_val (ARM_INSTR_LDM | 0x7FFF, &ptr);

  /* bx r8 */
  write_val (ARM_INSTR_BX | 0x8, &ptr);

  /* nop */
  write_val (ARM_INSTR_NOP, &ptr);

  for (i = 0; i < 15; i++)
    write_val (state.r[i], &ptr);

  err = grub_relocator_prepare_relocs (rel, get_physical_target_address(ch), &relst, &relsize);
  if (err)
    return err;

  grub_arch_sync_caches ((void *) relst, relsize);

  ((void (*) (void))  (relst)) ();

  /* Not reached.  */
  return GRUB_ERR_NONE;
}
