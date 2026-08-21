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

	.p2align	4	/* force 16-byte alignment */

VARIABLE (grub_relocator_forward_start)

	b copybegin1

VARIABLE (grub_relocator_forward_regs)
	.skip 12

copybegin1:
	ldr r0, [pc, #-20]
	ldr r1, [pc, #-20]
	ldr r2, [pc, #-20]

copycont1:
	ldrb r3, [r0]
	strb r3, [r1]
	add r0, r0, #1
	add r1, r1, #1
	subs r2, r2, #1
	bne copycont1

VARIABLE (grub_relocator_forward_end)

	.p2align	4	/* force 16-byte alignment */

VARIABLE (grub_relocator_backward_start)

	b copybegin2

VARIABLE (grub_relocator_backward_regs)
	.skip 12

copybegin2:
	ldr r0, [pc, #-20]
	ldr r1, [pc, #-20]
	ldr r2, [pc, #-20]

	add r0, r0, r2
	add r1, r1, r2

copycont2:
	sub r0, r0, #1
	sub r1, r1, #1
	ldrb r3, [r0]
	strb r3, [r1]
	subs r2, r2, #1
	bne copycont2

VARIABLE (grub_relocator_backward_end)
