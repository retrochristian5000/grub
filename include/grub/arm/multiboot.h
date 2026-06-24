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

#ifndef GRUB_MULTIBOOT_CPU_HEADER
#define GRUB_MULTIBOOT_CPU_HEADER	1

#define MULTIBOOT2_INITIAL_STATE  { .r[0] = MULTIBOOT2_BOOTLOADER_MAGIC, \
    .r[2] = 0,								  \
    .r[3] = 0,								  \
    }
#define MULTIBOOT_INITIAL_STATE  { .r[0] = MULTIBOOT_BOOTLOADER_MAGIC, \
    .r[2] = 0,								\
    .r[3] = 0,								\
    }
#define MULTIBOOT_ENTRY_REGISTER r[8]
#define MULTIBOOT_MBI_REGISTER r[1]
#define MULTIBOOT2_ARCHITECTURE_CURRENT MULTIBOOT2_ARCHITECTURE_ARM32

#ifdef GRUB_MACHINE_EFI

#define MULTIBOOT_EFI_INITIAL_STATE  { .r[0] = MULTIBOOT_BOOTLOADER_MAGIC, \
    .r[2] = 0,								    \
    .r[3] = 0,								    \
    }
#define MULTIBOOT2_EFI_INITIAL_STATE  { .r[0] = MULTIBOOT2_BOOTLOADER_MAGIC, \
    .r[2] = 0,								      \
    .r[3] = 0,								      \
    }
#define MULTIBOOT_EFI_ENTRY_REGISTER r[8]
#define MULTIBOOT_EFI_MBI_REGISTER r[1]

#endif /* GRUB_MACHINE_EFI */

#define MULTIBOOT_ELF32_MACHINE EM_ARM

#endif /* ! GRUB_MULTIBOOT_CPU_HEADER */
