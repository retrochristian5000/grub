/* bls.c - implementation of the boot loader spec */

/*
 *  GRUB  --  GRand Unified Bootloader
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

#include "grub/err.h"
#include <grub/charset.h>
#include <grub/extcmd.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/lib/envblk.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/filesystem.h>

#include <stddef.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define GRUB_BLS_CONFIG_PATH "\\loader\\entries\\"

static const grub_guid_t grub_simple_file_system_guid =
				GRUB_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static const grub_guid_t bli_vendor_guid =
				GRUB_EFI_VENDOR_BOOT_LOADER_INTERFACE_GUID;

/* Replace linux path delimiter (/) with EFI compatible (\) */
static void
use_efi_delimiter_for_path (char* s)
{
  char* tmp = grub_strchr(s, '/');
  while (tmp != NULL)
    {
      *tmp++ = '\\';
      tmp = grub_strchr(tmp, '/');
    }
}

/* Calculate the new filename with the bumped counter */
static char*
bump_counter (char* entry)
{
  char *tmp = grub_strrchr (entry, '+');
  int tries = -1, tries_left;
  *tmp = '\0';
  tries_left = grub_strtol (++tmp, (const char**) &tmp, 10);
  /* The parsing succeeded */
  if (grub_errno == GRUB_ERR_NONE)
    {
      if (tries_left > 0)
	{
	  tries_left--;
	}
      if (*tmp == '-')
	{
	  tmp++;
	  tries = grub_strtol (tmp, (const char**) &tmp, 10);
	  if (grub_errno == GRUB_ERR_NONE)
	    {
	      tries++;
	    } else {
	      tries = -1;
	    }
	}
    } else {
      return NULL;
    }

  if (tries == -1)
    {
      /* This is the first try, rename accordingly */
      return grub_xasprintf ("%s+%d-1.conf", entry, tries_left);
    } else {
      return grub_xasprintf ("%s+%d-%d.conf", entry, tries_left, tries);
    }
}

static grub_err_t
grub_cmd_bls_bumpcounter (grub_extcmd_context_t ctxt __attribute__ ((unused)),
			  int argc, char **args)
{
  grub_efi_loaded_image_t *image;
  grub_efi_file_info_t *file_info = NULL;
  grub_efi_file_protocol_t *handle, *root;
  grub_efi_simple_file_system_protocol_t *volume;
  grub_efi_boot_services_t *bs;
  grub_guid_t grub_efi_file_info_guid = GRUB_EFI_FILE_INFO_ID;
  grub_efi_status_t err;
  char *entry, *new_path = NULL, *old_path;
  const char *blsdir;
  grub_efi_uint64_t size;
  grub_efi_char16_t *old_path16;

  if (argc != 1) {
    grub_dprintf ("bls_bumpcounter", "only one argument should be passed\n");
    return GRUB_ERR_BAD_ARGUMENT;
  }
  entry = args[0];

  grub_efi_set_variable_to_string ("LoaderEntrySelected",
				   &bli_vendor_guid, entry,
				   GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS
				   | GRUB_EFI_VARIABLE_RUNTIME_ACCESS);

  /* Look for the start of the boot count.
     If no '+' symbol has been found, the boot counting isn't enabled for
     the selected entry */
  if (grub_strrchr (entry, '+') == NULL)
    {
      grub_dprintf ("bls_bumpcounter",
		    "boot counting is not in effect for %s\n", entry);
      return GRUB_ERR_NONE;
    }

  image = grub_efi_get_loaded_image (grub_efi_image_handle);

  if (!image)
    {
      grub_dprintf ("bls_bumpcounter", "grub_efi_get_loaded_image failed\n");
      return GRUB_ERR_BUG;
    }

  bs = grub_efi_system_table->boot_services;
  err = bs->handle_protocol (image->device_handle,
			     (void *) &grub_simple_file_system_guid, (void *) &volume);
  if (err != GRUB_EFI_SUCCESS)
    {
      grub_dprintf ("bls_bumpcounter",
		    "Cannot get handle to EFI_SIMPLE_FILE_SYSTEM_PROTOCOL: %lu\n",
		    (unsigned long) err);
      return GRUB_ERR_BAD_DEVICE; 
    }
  err = volume->OpenVolume (volume, &root);
  if (err != GRUB_EFI_SUCCESS)
    {
      grub_dprintf ("bls_bumpcounter",
	      "Cannot open the volume: %lu\n", (unsigned long) err);
      return GRUB_ERR_BAD_DEVICE;
    }

  blsdir = grub_env_get ("blsdir");
  if (blsdir)
    {
      blsdir = grub_strdup (blsdir);
    }
  if (blsdir)
    {
      use_efi_delimiter_for_path ((char*) blsdir);
    } else {
      blsdir = GRUB_BLS_CONFIG_PATH;
    }

  old_path = grub_xasprintf ("%s%s", blsdir, entry);
  grub_utf8_to_utf16_alloc (old_path, &old_path16, NULL);

  err = root->Open (root, &handle, old_path16,
		    GRUB_EFI_FILE_MODE_READ|GRUB_EFI_FILE_MODE_WRITE, 0);
  grub_free (old_path16);
  if (err != GRUB_EFI_SUCCESS)
    {
      grub_dprintf("bls_bumpcounter", "Cannot open the entry %s: %lu\n",
		   old_path, (unsigned long) err);
      grub_free (old_path);
      return GRUB_ERR_BAD_DEVICE;
    }
  grub_free (old_path);

  /* Get the file_info by allocating a fixed size that should be big enough for
     most cases, and, if it fails, allocate a new one with the exact size
     returned from GetInfo.
     Reference: systemd:src/boot/efi/util.c =*/
  size = offsetof (grub_efi_file_info_t, FileName)
		   + 256U * sizeof (grub_efi_char16_t);
  file_info = grub_malloc (size);
  if (!file_info)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto finish;
    }
  err = handle->GetInfo (handle, &grub_efi_file_info_guid, &size, file_info);
  if (err == GRUB_EFI_BUFFER_TOO_SMALL)
    {
      grub_free(file_info);
      /* When we bump the counter, it might happen that we increase the filename
         length. Add the space for 2 more characters. (i.e. adding the boot done
         part "-1") */
      size += 2;
      file_info = grub_malloc (size);
      if (!file_info)
	{
	  grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
	  goto finish;
	}
      err = handle->GetInfo (handle, &grub_efi_file_info_guid,
			     &size, file_info);
    }

  if (err != GRUB_EFI_SUCCESS)
    {
      grub_dprintf ("bls_bumpcounter",
		    "Cannot get the file_info of the entry: %lu\n", (unsigned long) err);
      goto finish;
    }

  new_path = bump_counter (entry);
  if (!new_path)
    goto finish;

  grub_dprintf ("bls_bumpcounter", "renaming entry to %s\n", new_path);
  grub_utf8_to_utf16 (file_info->FileName,
		      size - offsetof (grub_efi_file_info_t, FileName),
		      (grub_uint8_t*) new_path, grub_strlen (new_path), NULL);

  err = handle->SetInfo (handle, &grub_efi_file_info_guid, size, file_info);

  if (err != GRUB_EFI_SUCCESS)
    {
      grub_dprintf ("bls_bumpcounter", "Cannot rename file: %lu\n",
		    (unsigned long) err);
      goto finish;
    }

  handle->Flush (handle);
  grub_dprintf ("bls_bumpcounter", "entry renamed\n");
  handle->Close (handle);

  char* loader_boot_count_path = grub_xasprintf("%s%s", blsdir, new_path);
  grub_free(new_path);
  grub_efi_set_variable_to_string("LoaderBootCountPath", &bli_vendor_guid,
				  loader_boot_count_path,
				  GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS
				  | GRUB_EFI_VARIABLE_RUNTIME_ACCESS);
  grub_free(loader_boot_count_path);

  if (err != GRUB_EFI_SUCCESS)
    {
      grub_free(file_info);
      return GRUB_ERR_BUG;
    }

  grub_free(file_info);
  return GRUB_ERR_NONE;

finish:
  handle->Close (handle);
  grub_free(file_info);
  return GRUB_ERR_BUG;
}


static grub_extcmd_t cmd;

GRUB_MOD_INIT(blsbumpcounter)
{
  grub_dprintf ("bls_bumpcounter", "%s got here\n", __func__);
  cmd = grub_register_extcmd ("bls_bumpcounter",
			      grub_cmd_bls_bumpcounter,
			      0,
			      NULL,
			      N_("Bump the boot entry counting."),
			      NULL);
}

GRUB_MOD_FINI(blsbumpcounter)
{
  grub_unregister_extcmd (cmd);
}
