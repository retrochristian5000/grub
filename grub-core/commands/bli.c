/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2023  Free Software Foundation, Inc.
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
 *  Implementation of the Boot Loader Interface.
 */

#include <grub/charset.h>
#include <grub/efi/api.h>
#include <grub/efi/disk.h>
#include <grub/efi/efi.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/gpt_partition.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/partition.h>
#include <grub/tpm.h>
#include <grub/types.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define MODNAME "bli"

static const grub_guid_t bli_vendor_guid = GRUB_EFI_VENDOR_BOOT_LOADER_INTERFACE_GUID;

#define GRUB_EFI_LOADER_FEATURE_CONFIG_TIMEOUT          (1ULL << 0)
#define GRUB_EFI_LOADER_FEATURE_CONFIG_TIMEOUT_ONE_SHOT (1ULL << 1)
#define GRUB_EFI_LOADER_FEATURE_ENTRY_DEFAULT           (1ULL << 2)
#define GRUB_EFI_LOADER_FEATURE_ENTRY_ONESHOT           (1ULL << 3)
#define GRUB_EFI_LOADER_FEATURE_BOOT_COUNTING           (1ULL << 4)
#define GRUB_EFI_LOADER_FEATURE_XBOOTLDR                (1ULL << 5)
#define GRUB_EFI_LOADER_FEATURE_MENU_DISABLE            (1ULL << 13)
#define GRUB_EFI_LOADER_FEATURE_TPM2_ACTIVE_PCR_BANKS   (1ULL << 18)

static grub_err_t
set_loader_features (void)
{
  static long loader_features =
      GRUB_EFI_LOADER_FEATURE_CONFIG_TIMEOUT |
      GRUB_EFI_LOADER_FEATURE_CONFIG_TIMEOUT_ONE_SHOT |
      GRUB_EFI_LOADER_FEATURE_ENTRY_DEFAULT |
      GRUB_EFI_LOADER_FEATURE_ENTRY_ONESHOT |
      GRUB_EFI_LOADER_FEATURE_BOOT_COUNTING |
      GRUB_EFI_LOADER_FEATURE_XBOOTLDR |
      GRUB_EFI_LOADER_FEATURE_MENU_DISABLE |
      GRUB_EFI_LOADER_FEATURE_TPM2_ACTIVE_PCR_BANKS;

  return grub_efi_set_variable("LoaderFeatures", &bli_vendor_guid, &loader_features, sizeof(loader_features));
}

static grub_err_t
get_part_uuid (const char *device_name, char **part_uuid)
{
  grub_device_t device;
  grub_err_t status = GRUB_ERR_NONE;
  grub_disk_t disk = NULL;
  struct grub_gpt_partentry entry;

  device = grub_device_open (device_name);
  if (device == NULL)
    return grub_error (grub_errno, N_("cannot open device: %s"), device_name);

  if (device->disk == NULL)
    {
      grub_dprintf ("bli", "%s is not a disk device, partuuid skipped\n", device_name);
      *part_uuid = NULL;
      grub_device_close (device);
      return GRUB_ERR_NONE;
    }

  if (device->disk->partition == NULL)
    {
      grub_dprintf ("bli", "%s has no partition, partuuid skipped\n", device_name);
      *part_uuid = NULL;
      grub_device_close (device);
      return GRUB_ERR_NONE;
    }

  disk = grub_disk_open (device->disk->name);
  if (disk == NULL)
    {
      status = grub_error (grub_errno, N_("cannot open disk: %s"), device_name);
      grub_device_close (device);
      return status;
    }

  if (grub_strcmp (device->disk->partition->partmap->name, "gpt") != 0)
    {
      grub_dprintf ("bli", "%s is not a GPT partition, partuuid skipped\n", device_name);
      *part_uuid = NULL;
      grub_disk_close (disk);
      grub_device_close (device);
      return GRUB_ERR_NONE;
    }

  if (grub_disk_read (disk, device->disk->partition->offset,
		      device->disk->partition->index, sizeof (entry), &entry) != GRUB_ERR_NONE)
    {
      status = grub_error (grub_errno, N_("read error: %s"), device_name);
      goto fail;
    }

  *part_uuid = grub_xasprintf ("%pG", &entry.guid);
  if (*part_uuid == NULL)
    status = grub_errno;

 fail:
  grub_disk_close (disk);
  grub_device_close (device);

  return status;
}

static grub_err_t
set_loader_device_part_uuid (void)
{
  grub_efi_loaded_image_t *image;
  char *device_name;
  grub_err_t status = GRUB_ERR_NONE;
  char *part_uuid = NULL;

  image = grub_efi_get_loaded_image (grub_efi_image_handle);
  if (image == NULL)
    return grub_error (GRUB_ERR_BAD_DEVICE, N_("unable to find boot device"));

  device_name = grub_efidisk_get_device_name (image->device_handle);
  if (device_name == NULL)
    return grub_error (GRUB_ERR_BAD_DEVICE, N_("unable to find boot device"));

  status = get_part_uuid (device_name, &part_uuid);

  if (status == GRUB_ERR_NONE && part_uuid)
    status = grub_efi_set_variable_to_string ("LoaderDevicePartUUID", &bli_vendor_guid, part_uuid,
					      GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS |
					      GRUB_EFI_VARIABLE_RUNTIME_ACCESS);
  else if (status != GRUB_ERR_NONE)
    grub_error (status, N_("unable to determine partition UUID of boot device"));

  grub_free (part_uuid);
  grub_free (device_name);
  return status;
}

static grub_err_t
set_loader_active_pcr_banks (void)
{
  grub_efi_uint32_t active_pcr_banks;
  char *active_pcr_banks_str;
  grub_err_t status;

  active_pcr_banks = grub_tpm2_active_pcr_banks();
  active_pcr_banks_str = grub_xasprintf ("0x%08x", active_pcr_banks);
  if (active_pcr_banks_str == NULL)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("cannot allocate active PCR banks string"));

  status = grub_efi_set_variable_to_string ("LoaderTpm2ActivePcrBanks",
					     &bli_vendor_guid,
					     active_pcr_banks_str,
					     GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS |
					     GRUB_EFI_VARIABLE_RUNTIME_ACCESS);
  grub_free (active_pcr_banks_str);
  return status;
}

/* Get the value of a EFI variable */
static char*
get_efivar (const char* efivar)
{
  grub_efi_status_t status;
  grub_size_t size;
  grub_efi_char16_t *val_efi = NULL;
  char *val = NULL;
  status = grub_efi_get_variable (efivar,
				  &bli_vendor_guid,
				  &size,
				  (void**) &val_efi);
  if (status == GRUB_EFI_SUCCESS && size != 0)
    {
      val = grub_malloc (size * sizeof (char));
      grub_utf16_to_utf8 ((grub_uint8_t*) val,
			  (grub_uint16_t*) val_efi, size);
    }
  return val;
}

static void
set_loader_default_entry (void)
{
  char* default_entry;
  char* oneshot_entry = get_efivar("LoaderEntryOneShot");

  if (oneshot_entry != NULL)
    {
      grub_env_set ("default", oneshot_entry);
      grub_free (oneshot_entry);
      grub_efi_set_variable_to_string("LoaderEntryOneShot",
				      &bli_vendor_guid, "", 0);
    }
  else
    {
      default_entry = get_efivar("LoaderEntryDefault");
      if (default_entry != NULL)
	{
	  grub_env_set ("default", default_entry);
      grub_free (default_entry);
	}
    }
}

/* Parse the timeout according to the Boot Loader Interface spec
   for LoaderConfigTimeout and LoaderConfigTimeoutOneShot.
   Return true when the timeout has been set. */
static int
parse_timeout_bli (const char* config_timeout, bool is_oneshot)
{
  unsigned int timeout;
  /* When the timeout is set to menu-force or LoaderConfigTimeoutOneShot is
     set to 0, show the menu indefinitely */
  if (grub_strcmp (config_timeout, "menu-force") == 0
          || (is_oneshot && grub_strcmp (config_timeout, "0") == 0))
    {
      grub_env_unset ("timeout");
      grub_env_set ("timeout_style", "menu");
      /* When set to 0 or to menu-hidden, don't show the menu but wait until the
         timeout expires. Allow the user to interrupt the boot and show the menu */
    } else if (grub_strcmp (config_timeout, "0") == 0
	    || grub_strcmp (config_timeout, "menu-hidden") == 0)
    {
      grub_menu_set_timeout (3);
      grub_env_set ("timeout_style", "hidden");
      /* When set to to menu-disabled, don't show the menu and boot the default
         entry directly. Don't allow the user to interrupt the bootloader. */
    } else if (grub_strcmp (config_timeout, "menu-disabled") == 0)
    {
      grub_env_set ("timeout", "0");
      grub_env_set ("timeout_style", "disabled");
    } else {
      timeout = grub_strtoul (config_timeout, 0, 10);
      if (grub_errno == GRUB_ERR_NONE)
	{
          /* In previous versions of bootctl, timeout is set to GRUB_UINT_MAX
             instead of menu-force */
	  if (timeout == GRUB_UINT_MAX)
	    grub_env_unset ("timeout");
	  else
	    grub_menu_set_timeout (timeout);

	  grub_env_set ("timeout_style", "menu");
	} else {
	  grub_errno = GRUB_ERR_NONE;
          return false;
	}
    }

    return true;
}

/* Read LoaderConfigTimeout and LoaderConfigTimeoutOneShot and update
   timeout accordingly */
static void
set_timeout_from_loader_config (void)
{
  bool has_oneshot = false;
  char* config_timeout = get_efivar ("LoaderConfigTimeoutOneShot");
  if (config_timeout)
  {
    /* Remove its value, regardless if it is valid or not */
    grub_efi_set_variable_to_string ("LoaderConfigTimeoutOneShot",
	    &bli_vendor_guid, "", 0);

    has_oneshot = parse_timeout_bli (config_timeout, true);
    grub_free (config_timeout);
  }

  if (has_oneshot)
    return;

  /* If there was no LoaderConfigTimeoutOneShot value or it was invalid,
     read LoaderConfigTimeout */
  config_timeout = get_efivar ("LoaderConfigTimeout");
  if (!config_timeout)
    return;

  parse_timeout_bli (config_timeout, false);
  grub_free (config_timeout);
}

GRUB_MOD_INIT (bli)
{
  grub_efi_set_variable_to_string ("LoaderInfo", &bli_vendor_guid, PACKAGE_STRING,
				   GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS |
				   GRUB_EFI_VARIABLE_RUNTIME_ACCESS);
  set_loader_device_part_uuid ();
  set_loader_active_pcr_banks ();
  set_loader_default_entry ();
  set_timeout_from_loader_config ();
  set_loader_features ();
  /* No error here is critical, other than being logged */
  grub_print_error ();
}
