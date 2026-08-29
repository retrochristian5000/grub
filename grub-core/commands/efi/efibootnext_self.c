/* efibootnext_self.c - Identify the EFI entry that boots the running grub. */
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
 *
 *  The firmware entry that boots this grub must not be offered as a circular
 *  BootNext target.  BootCurrent only names the firmware's first-stage boot
 *  and is blind to chainloading, so identify self from the running image
 *  instead -- its ESP partition signature and the \EFI\<vendor>\ directory it
 *  was loaded from.  An entry is self when both match, mirroring the host-side
 *  detection in 31_efi_bootnext.
 */

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>

#include "efibootnext.h"

/* Total byte length of a trusted, End-terminated device path.  */
static grub_size_t
device_path_size (const grub_efi_device_path_t *dp)
{
  const grub_efi_device_path_t *p = dp;

  while (GRUB_EFI_DEVICE_PATH_VALID (p)
         && GRUB_EFI_DEVICE_PATH_TYPE (p) != GRUB_EFI_END_DEVICE_PATH_TYPE)
    p = GRUB_EFI_NEXT_DEVICE_PATH (p);
  if (!GRUB_EFI_DEVICE_PATH_VALID (p))
    return 0;
  return (const grub_uint8_t *) p - (const grub_uint8_t *) dp
         + GRUB_EFI_DEVICE_PATH_LENGTH (p);
}

/*
 * First node of the given type/subtype, or NULL.  A match is returned only
 * when the whole path is well-formed: every node header in bounds and an
 * End node reached within len bytes.  Callers may therefore hand the path
 * to walkers that trust their input (e.g. grub_efi_get_filename).
 */
static const grub_efi_device_path_t *
device_path_find (const grub_efi_device_path_t *dp, grub_size_t len,
                  grub_efi_uint8_t type, grub_efi_uint8_t subtype)
{
  const grub_uint8_t *base = (const grub_uint8_t *) dp;
  const grub_uint8_t *p = base;
  const grub_efi_device_path_t *found = NULL;

  while (p + 4 <= base + len)
    {
      const grub_efi_device_path_t *node = (const grub_efi_device_path_t *) p;
      grub_efi_uint16_t nlen = GRUB_EFI_DEVICE_PATH_LENGTH (node);

      if (nlen < 4 || p + nlen > base + len)
        return NULL;
      if (GRUB_EFI_DEVICE_PATH_TYPE (node) == GRUB_EFI_END_DEVICE_PATH_TYPE)
        return found;
      if (found == NULL && GRUB_EFI_DEVICE_PATH_TYPE (node) == type
          && GRUB_EFI_DEVICE_PATH_SUBTYPE (node) == subtype)
        found = node;
      p += nlen;
    }
  return NULL;
}

/*
 * The first path component after a leading "EFI" segment, lowercased, e.g.
 * "/EFI/fedora/grubx64.efi" -> "fedora".  grub_efi_get_filename normalises the
 * separators to '/'.  Returns NULL when there is no such component.
 */
static char *
vendor_from_filename (const char *path)
{
  const char *seg, *next;
  int after_efi = 0;

  for (seg = path; *seg != '\0'; seg = next)
    {
      grub_size_t len, i;

      while (*seg == '/')
        seg++;
      next = seg;
      while (*next != '\0' && *next != '/')
        next++;
      len = next - seg;
      if (len == 0)
        break;

      if (after_efi)
        {
          char *v = grub_malloc (len + 1);

          if (v == NULL)
            return NULL;
          for (i = 0; i < len; i++)
            v[i] = grub_tolower ((grub_uint8_t) seg[i]);
          v[len] = '\0';
          return v;
        }
      if (len == 3 && grub_tolower ((grub_uint8_t) seg[0]) == 'e'
          && grub_tolower ((grub_uint8_t) seg[1]) == 'f'
          && grub_tolower ((grub_uint8_t) seg[2]) == 'i')
        after_efi = 1;
    }
  return NULL;
}

void
grub_efibootnext_self_init (struct self_id *s)
{
  grub_efi_loaded_image_t *img;
  grub_efi_device_path_t *dp;
  char *fname;

  grub_memset (s, 0, sizeof (*s));

  img = grub_efi_get_loaded_image (grub_efi_image_handle);
  if (img == NULL)
    return;

  dp = grub_efi_get_device_path (img->device_handle);
  if (dp != NULL)
    {
      const grub_efi_hard_drive_device_path_t *hd;

      hd = (const grub_efi_hard_drive_device_path_t *)
           device_path_find (dp, device_path_size (dp),
                             GRUB_EFI_MEDIA_DEVICE_PATH_TYPE,
                             GRUB_EFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE);
      if (hd != NULL)
        {
          grub_memcpy (s->sig, hd->partition_signature, sizeof (s->sig));
          s->sig_type = hd->signature_type;
          s->have_part = 1;
        }
    }

  fname = grub_efi_get_filename (img->file_path);
  grub_errno = GRUB_ERR_NONE;
  if (fname != NULL)
    {
      s->vendor = vendor_from_filename (fname);
      grub_free (fname);
    }
}

void
grub_efibootnext_self_fini (struct self_id *s)
{
  grub_free (s->vendor);
  s->vendor = NULL;
}

int
grub_efibootnext_self_known (const struct self_id *s)
{
  return s->have_part && s->vendor != NULL;
}

int
grub_efibootnext_is_self (const void *fpl, grub_size_t fpl_len,
                          const struct self_id *s)
{
  const grub_efi_device_path_t *dp = fpl;
  const grub_efi_hard_drive_device_path_t *hd;
  char *fname, *vendor;
  int match = 0;

  if (!grub_efibootnext_self_known (s) || fpl == NULL)
    return 0;

  hd = (const grub_efi_hard_drive_device_path_t *)
       device_path_find (dp, fpl_len, GRUB_EFI_MEDIA_DEVICE_PATH_TYPE,
                         GRUB_EFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE);
  if (hd == NULL || hd->signature_type != s->sig_type
      || grub_memcmp (hd->partition_signature, s->sig, sizeof (s->sig)) != 0)
    return 0;

  fname = grub_efi_get_filename ((grub_efi_device_path_t *) dp);
  grub_errno = GRUB_ERR_NONE;
  if (fname == NULL)
    return 0;
  vendor = vendor_from_filename (fname);
  grub_free (fname);
  if (vendor != NULL && grub_strcmp (vendor, s->vendor) == 0)
    match = 1;
  grub_free (vendor);
  return match;
}
