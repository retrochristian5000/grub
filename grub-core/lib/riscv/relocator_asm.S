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
 */

#include <grub/symbol.h>

#if __riscv_xlen == 64
#define LOAD_IDX(reg, idx)	ld reg, (idx*8)(t3)
#else
#define LOAD_IDX(reg, idx)	lw reg, (idx*4)(t3)
#endif

	.p2align	4	/* force 16-byte alignment */

VARIABLE (grub_relocator_forward_start)

	j copybegin1

	.p2align	4

VARIABLE (grub_relocator_forward_regs)
	.skip __riscv_xlen / 8 * 3

copybegin1:
	la t3, grub_relocator_forward_regs
	LOAD_IDX(t0, 0)
	LOAD_IDX(t1, 1)
	LOAD_IDX(t2, 2)

copycont1:
	lb t3, (t0)
	sb t3, (t1)
	addi t0, t0, 1
	addi t1, t1, 1
	addi t2, t2, -1
	bnez t2, copycont1

VARIABLE (grub_relocator_forward_end)

	.p2align	4	/* force 16-byte alignment */

VARIABLE (grub_relocator_backward_start)

	j copybegin2

	.p2align	4

VARIABLE (grub_relocator_backward_regs)
	.skip __riscv_xlen / 8 * 3

copybegin2:
	la t3, grub_relocator_backward_regs
	LOAD_IDX(t0, 0)
	LOAD_IDX(t1, 1)
	LOAD_IDX(t2, 2)

	add t0, t0, t2
	add t1, t1, t2

copycont2:
	addi t0, t0, -1
	addi t1, t1, -1
	lb t3, (t0)
	sb t3, (t1)
	addi t2, t2, -1
	bnez t2, copycont2

VARIABLE (grub_relocator_backward_end)
