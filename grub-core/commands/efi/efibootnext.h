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
 * Identity of the firmware entry that boots the running grub, used to keep
 * efibootmenu from offering its own entry as a circular BootNext target.
 * Derived from the running image rather than BootCurrent (which is blind to
 * chainloading).  Implemented in efibootnext_self.c.
 */
struct self_id
{
  int have_part;			/* the partition signature below is valid */
  grub_efi_uint8_t sig[16];
  grub_efi_uint8_t sig_type;
  char *vendor;				/* lowercased \EFI\<vendor>\ dir, or NULL */
};

/* Fill in self from the running image; partial results are fine.  */
void grub_efibootnext_self_init (struct self_id *s);

/* Release anything self_init allocated.  */
void grub_efibootnext_self_fini (struct self_id *s);

/* True when self is fully determined (both halves of the match available).  */
int grub_efibootnext_self_known (const struct self_id *s);

/*
 * True when the given FilePathList (as returned by
 * grub_efibootnext_decode_option, so already bounds-checked) boots the
 * running grub.
 */
int grub_efibootnext_is_self (const void *fpl, grub_size_t fpl_len,
			      const struct self_id *s);

/*
 * Decode an EFI_LOAD_OPTION: return its Description as a malloc'd UTF-8 string
 * (NULL when absent, unterminated or on error) and, when attrs_out is
 * non-NULL, its Attributes word.  When fpl_out/fpl_len_out are non-NULL they
 * receive the FilePathList, validated to lie within the option buffer (NULL/0
 * when it does not).  Defined in efibootnext.c, shared with the menu command.
 */
grub_uint8_t *grub_efibootnext_decode_option (const void *option,
					      grub_size_t size,
					      grub_uint32_t *attrs_out,
					      const void **fpl_out,
					      grub_size_t *fpl_len_out);

/* Register/unregister the efibootmenu command; defined in efibootnext_menu.c.  */
void grub_efibootnext_menu_init (void);
void grub_efibootnext_menu_fini (void);

#endif /* ! GRUB_EFIBOOTNEXT_HEADER */
