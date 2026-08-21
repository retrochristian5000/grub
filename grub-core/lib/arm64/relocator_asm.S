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

copycont1:
	ldrb w11, [x8]
	strb w11, [x9]
	add x8, x8, #1
	add x9, x9, #1
	subs x10, x10, #1
	bne copycont1

VARIABLE (grub_relocator_forward_end)

	.p2align	4	/* force 16-byte alignment */

VARIABLE (grub_relocator_backward_start)

	add x9, x9, x10
	add x8, x8, x10
	sub x9, x9, #1
	sub x8, x8, #1

copycont2:
	ldrb w11, [x8]
	strb w11, [x9]
	sub x8, x8, #1
	sub x9, x9, #1
	subs x10, x10, #1
	bne copycont2

VARIABLE (grub_relocator_backward_end)
