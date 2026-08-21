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

#if __riscv_xlen == 64
#include <grub/riscv64/relocator.h>
#else
#include <grub/riscv32/relocator.h>
#endif
#include <grub/relocator_private.h>

#define CONCAT(a,b)     CONCAT_(a, b)
#define CONCAT_(a,b)    a ## b

/* Defines to simplify creating machine code instructions */

#define RV32I_INSTR_JALR	0x00000067
#define RV32I_INSTR_AUIPC	0x00000017
#define RV32I_INSTR_ADDI	0x00000013
#define RV32I_INSTR_LW		0x00002003
#define RV64I_INSTR_LD		0x00003003

#define RISCV_REG_T0	5
#define RISCV_REG_T6	31
#define RISCV_REG_A1	11

extern grub_uint8_t grub_relocator_forward_start;
extern grub_uint8_t grub_relocator_forward_end;
extern grub_uint8_t grub_relocator_forward_regs;
extern grub_uint8_t grub_relocator_backward_start;
extern grub_uint8_t grub_relocator_backward_end;
extern grub_uint8_t grub_relocator_backward_regs;
extern grub_uint32_t grub_relocator_check_endian;

#define REGW_SIZEOF (1 * sizeof (grub_size_t) + sizeof (grub_uint32_t))
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

#if __riscv_xlen == 64
static void
write_u64 (grub_uint64_t val, void **target)
{
  *(grub_uint64_t *) *target = val;
  *target = ((grub_uint64_t *) *target) + 1;
}
#endif

static void
write_u32 (grub_uint32_t val, void **target)
{
  *(grub_uint32_t *) *target = val;
  *target = ((grub_uint32_t *) *target) + 1;
}

static void
write_imm (grub_size_t val, void **target)
{
#if __riscv_xlen == 64
  write_u64 (val, target);
#else
  write_u32 (val, target);
#endif
}

static void
read_reg (grub_uint8_t rd, grub_uint8_t rs, grub_uint16_t off, void **target)
{
#if __riscv_xlen == 64
  /* ld rd, off(rs) */
  write_u32 (RV64I_INSTR_LD | (rd << 7) | (rs << 15) | (off << 20), target);
#else
  /* lw rd, off(rs) */
  write_u32 (RV32I_INSTR_LW | (rd << 7) | (rs << 15) | (off << 20), target);
#endif
}

void
grub_cpu_relocator_jumper (void *rels, grub_addr_t addr)
{
  /* auipc t0, 0 */
  write_u32 (RV32I_INSTR_AUIPC | (RISCV_REG_T0 << 7), &rels);

  /* addi t0, t0, 16 */
  write_u32 (RV32I_INSTR_ADDI | (RISCV_REG_T0 << 7) | (RISCV_REG_T0 << 15) | (16 << 20), &rels);

  read_reg (RISCV_REG_T0, RISCV_REG_T0, 0, &rels);

  /* jr t0 */
  write_u32 (RV32I_INSTR_JALR | (RISCV_REG_T0 << 15), &rels);

  /* immediate value */
  write_imm (addr, &rels);
}

void
grub_cpu_relocator_backward (void *ptr, void *src, void *dest,
			     grub_size_t size)
{
  grub_memcpy (ptr, &grub_relocator_backward_start,
	       RELOCATOR_SIZEOF (backward));
  ptr = (grub_uint8_t *) ptr + RELOCATOR_REGS_OFFSET (backward);
  write_imm ((grub_size_t) src, &ptr);
  write_imm ((grub_size_t) dest, &ptr);
  write_imm ((grub_size_t) size, &ptr);
}

void
grub_cpu_relocator_forward (void *ptr, void *src, void *dest,
			    grub_size_t size)
{
  grub_memcpy (ptr, &grub_relocator_forward_start,
	       RELOCATOR_SIZEOF (forward));
  ptr = (grub_uint8_t *) ptr + RELOCATOR_REGS_OFFSET (forward);
  write_imm ((grub_size_t) src, &ptr);
  write_imm ((grub_size_t) dest, &ptr);
  write_imm ((grub_size_t) size, &ptr);
}

void
grub_cpu_relocator_preamble (void *rels __attribute__((unused)))
{
}

grub_err_t
CONCAT(CONCAT(grub_relocator, __riscv_xlen), _boot) (struct grub_relocator *rel,
		       struct CONCAT(CONCAT(grub_relocator, __riscv_xlen), _state) state)
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

  /* auipc t6, 0 */
  write_u32 (RV32I_INSTR_AUIPC | (RISCV_REG_T6 << 7), &ptr);

  /* addi t6, t6, 140 */
  write_u32 (RV32I_INSTR_ADDI | (RISCV_REG_T6 << 7) | (RISCV_REG_T6 << 15) | (140 << 20), &ptr);

  for (i = 0; i < 32; i++)
    read_reg (i, RISCV_REG_T6, i * __riscv_xlen / 8, &ptr);

  /* jalr zero, a1, 0 */
  write_u32 (RV32I_INSTR_JALR | (RISCV_REG_A1 << 15), &ptr);

  for (i = 0; i < 32; i++)
    write_imm (state.x[i], &ptr);

  err = grub_relocator_prepare_relocs (rel, get_physical_target_address(ch), &relst, &relsize);
  if (err)
    return err;

  grub_arch_sync_caches ((void *) relst, relsize);

  ((void (*) (void)) relst) ();

  /* Not reached.  */
  return GRUB_ERR_NONE;
}
