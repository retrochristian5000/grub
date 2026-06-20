/* efibootnext.h - shared declarations within the efibootnext module. */
/*
 *  Copyright (C) 2026  Free Software Foundation, Inc.
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
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_EFIBOOTNEXT_HEADER
#define GRUB_EFIBOOTNEXT_HEADER 1

#include <grub/types.h>
#include <grub/efi/api.h>

/*
 * Decode an EFI_LOAD_OPTION: return its Description as a malloc'd UTF-8 string
 * (NULL when absent, unterminated or on error) and, when attrs_out is
 * non-NULL, its Attributes word.  Defined in efibootnext.c, shared with the
 * menu command.
 */
grub_uint8_t *grub_efibootnext_decode_option (const void *option,
					      grub_size_t size,
					      grub_uint32_t *attrs_out);

/* Register/unregister the efibootmenu command; defined in efibootnext_menu.c.  */
void grub_efibootnext_menu_init (void);
void grub_efibootnext_menu_fini (void);

#endif /* ! GRUB_EFIBOOTNEXT_HEADER */
