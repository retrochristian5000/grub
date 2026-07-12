/* efibootnext_menu.c - The efibootmenu command: a menu from EFI boot options. */
/*
 *  Copyright (C) 2026  Free Software Foundation, Inc.
 *  Copyright (C) 2026  Thomas Grainger <tagrain@gmail.com>
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
 *  efibootmenu [OPTIONS]
 *    Discover the firmware boot options at boot time and add one GRUB menu
 *    entry per Boot#### that sets BootNext and reboots.  Intended to be placed
 *    in grub.cfg instead of generating static entries at config time.
 */

#include <grub/charset.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/normal.h>
#include <grub/types.h>

#include "efibootnext.h"

/* Read a single UINT16 global variable (BootCurrent).  Returns 1 on success.  */
static int
get_boot_uint16 (const char *name, grub_uint16_t *out)
{
  grub_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  void *data = NULL;
  grub_size_t size = 0;
  int ok = 0;

  if (grub_efi_get_variable (name, &global, &size, &data) == GRUB_EFI_SUCCESS
      && data != NULL && size >= sizeof (*out))
    {
      grub_memcpy (out, data, sizeof (*out));
      ok = 1;
    }
  grub_free (data);
  return ok;
}

/*
 * Derive a theme --class from the first word of the label, lowercased, with
 * trailing digits dropped and remaining non-alphanumerics turned into '_'
 * (e.g. "Windows Boot Manager" -> "windows").  Returns NULL when nothing
 * usable remains.  This is the C version of the same --class derivation
 * done in util/grub.d/30_os-prober.in.
 */
static char *
derive_class (const char *label)
{
  grub_size_t n = 0, i;
  char *out;

  while (label[n] != '\0' && label[n] != ' ')
    n++;

  out = grub_malloc (n + 1);
  if (out == NULL)
    return NULL;

  for (i = 0; i < n; i++)
    out[i] = grub_tolower ((grub_uint8_t) label[i]);
  out[n] = '\0';

  /* Drop trailing digits, e.g. "windows10" -> "windows".  */
  while (n > 0 && grub_isdigit ((grub_uint8_t) out[n - 1]))
    out[--n] = '\0';

  /* Map anything outside [a-z0-9_] to '_'.  */
  for (i = 0; i < n; i++)
    if (!grub_isalnum ((grub_uint8_t) out[i]) && out[i] != '_')
      out[i] = '_';

  if (n == 0)
    {
      grub_free (out);
      return NULL;
    }
  return out;
}

struct bootmenu_opts
{
  grub_uint16_t current;
  int have_current;
  int omit_inactive;
  int savedefault;
  int category_boot_only;
  struct self_id self;
};

/*
 * Read Boot####, apply the active/hidden/category filters and, if it passes,
 * add a menu entry that sets BootNext to it and reboots.
 */
static void
add_bootnext_entry (grub_uint16_t num, const struct bootmenu_opts *o)
{
  grub_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  char varname[sizeof ("Boot####")];
  void *load_option = NULL;
  grub_size_t load_option_size = 0;
  const void *fpl = NULL;
  grub_size_t fpl_len = 0;
  grub_uint32_t attrs = 0;
  grub_uint8_t *label = NULL;
  char *title = NULL, *id = NULL, *src = NULL, *class_main = NULL;
  int is_self;

  grub_snprintf (varname, sizeof (varname), "Boot%04X", (unsigned) num);
  /*
   * Enumeration is best effort: a Boot#### that cannot be read is skipped so
   * one unreadable entry does not drop the rest of the menu.
   */
  if (grub_efi_get_variable (varname, &global, &load_option_size, &load_option)
        != GRUB_EFI_SUCCESS || load_option == NULL)
    goto finish;

  label = grub_efibootnext_decode_option (load_option, load_option_size,
                                          &attrs, &fpl, &fpl_len);
  grub_errno = GRUB_ERR_NONE;

  if (label == NULL || *label == '\0')
    goto finish;
  /* HIDDEN entries are kept out of the firmware's own boot menu; honour that.  */
  if ((attrs & GRUB_EFI_LOAD_OPTION_HIDDEN) != 0)
    goto finish;
  /* Entries without ACTIVE are not auto-booted by the firmware.  */
  if (o->omit_inactive && (attrs & GRUB_EFI_LOAD_OPTION_ACTIVE) == 0)
    goto finish;
  /* CATEGORY_BOOT entries are normal boot targets; APP/reserved ones are not.  */
  if (o->category_boot_only
      && (attrs & GRUB_EFI_LOAD_OPTION_CATEGORY) != GRUB_EFI_LOAD_OPTION_CATEGORY_BOOT)
    goto finish;

  /*
   * Skip the entry that boots this grub.  Identify it from the running image
   * (ESP + vendor dir); only when that is unavailable fall back to BootCurrent,
   * which cannot see a chainload and so is a best-effort backstop.  Checked
   * after the cheap filters above: the device-path walk is the costly test.
   */
  if (grub_efibootnext_self_known (&o->self))
    is_self = grub_efibootnext_is_self (fpl, fpl_len, &o->self);
  else
    is_self = (o->have_current && num == o->current);
  grub_errno = GRUB_ERR_NONE;
  if (is_self)
    goto finish;

  class_main = derive_class ((const char *) label);
  title = grub_xasprintf ("%s (EFI BootNext)", label);
  id = grub_xasprintf ("efi-bootnext-%04X", (unsigned) num);
  /*
   * savedefault runs only on a successful bootnext, so a rejected one-shot
   * (e.g. a vanished entry) does not update grubenv.
   */
  src = grub_xasprintf ("if bootnext %04X; then\n%sreboot\nfi\n",
                        (unsigned) num, o->savedefault ? "savedefault\n" : "");

  if (title != NULL && id != NULL && src != NULL)
    {
      const char *argv[1];
      char *classes[3];

      argv[0] = title;
      classes[0] = (class_main != NULL) ? class_main : (char *) "efi";
      classes[1] = (char *) "os";
      classes[2] = NULL;

      /*
       * users "" restricts the entry to superusers whenever authentication
       * is enabled, the same default a config-time menuentry without --users
       * gets; NULL would leave it selectable without authentication.
       *
       * Best effort: a failed entry is skipped rather than aborting the whole
       * menu, so clear any error it left behind and carry on with the rest.
       */
      grub_normal_add_menu_entry (1, argv, classes, id, "", NULL, NULL,
                                  src, 0, NULL);
      grub_errno = GRUB_ERR_NONE;
    }

 finish:
  grub_free (load_option);
  grub_free (class_main);
  grub_free (title);
  grub_free (id);
  grub_free (src);
  grub_free (label);
}

/* Parse a "Boot####" variable name; returns 1 and the number on success.  */
static int
parse_boot_varname (const char *name, grub_uint16_t *num)
{
  const char *end;
  unsigned long v;

  if (grub_strlen (name) != sizeof ("Boot####") - 1
      || grub_strncmp (name, "Boot", 4) != 0)
    return 0;

  /* grub_strtoul sets grub_errno on failure but never clears it.  */
  grub_errno = GRUB_ERR_NONE;
  v = grub_strtoul (name + 4, &end, 16);
  if (grub_errno != GRUB_ERR_NONE || *end != '\0' || v > 0xffff)
    {
      grub_errno = GRUB_ERR_NONE;
      return 0;
    }
  *num = (grub_uint16_t) v;
  return 1;
}

/*
 * Add entries for any Boot#### variable not already covered by BootOrder, in
 * firmware enumeration order.  seen tracks which numbers were handled.
 */
static void
add_remaining_entries (grub_uint8_t *seen, const struct bootmenu_opts *o)
{
  grub_efi_runtime_services_t *r = grub_efi_system_table->runtime_services;
  grub_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  grub_efi_uintn_t name_size, buf_size = 256;
  grub_efi_char16_t *name;
  grub_guid_t guid;

  grub_memset (&guid, 0, sizeof (guid));
  name = grub_calloc (buf_size, 1);
  if (name == NULL)
    return;
  name[0] = 0;

  while (1)
    {
      grub_efi_status_t st;
      grub_size_t n16;
      grub_uint8_t u8[(sizeof ("Boot####")) * GRUB_MAX_UTF8_PER_UTF16 + 1];
      grub_uint16_t num;

      name_size = buf_size;
      st = r->get_next_variable_name (&name_size, name, &guid);
      if (st == GRUB_EFI_BUFFER_TOO_SMALL)
        {
          grub_efi_char16_t *tmp = grub_realloc (name, name_size);
          if (tmp == NULL)
            break;
          name = tmp;
          buf_size = name_size;
          continue;
        }
      if (st != GRUB_EFI_SUCCESS)
        break;

      if (grub_memcmp (&guid, &global, sizeof (guid)) != 0)
        continue;

      /* name_size counts the terminating NUL; skip names too long to match.  */
      n16 = name_size / sizeof (grub_efi_char16_t);
      if (n16 == 0 || n16 > sizeof ("Boot####"))
        continue;

      *grub_utf16_to_utf8 (u8, name, n16) = '\0';
      if (!parse_boot_varname ((const char *) u8, &num))
        continue;
      if ((seen[num >> 3] & (1 << (num & 7))) != 0)
        continue;
      seen[num >> 3] |= 1 << (num & 7);
      add_bootnext_entry (num, o);
    }

  grub_free (name);
}

static const struct grub_arg_option efibootmenu_options[] =
{
  {"omit-inactive", 0, 0, N_("Skip boot entries not marked active."), 0, 0},
  {"savedefault", 0, 0,
   N_("Run savedefault before rebooting into the chosen entry."), 0, 0},
  {"category-boot-only", 0, 0,
   N_("Skip entries not in the boot category (e.g. firmware apps)."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum
{
  OPT_OMIT_INACTIVE,
  OPT_SAVEDEFAULT,
  OPT_CATEGORY_BOOT_ONLY
};

static grub_err_t
grub_cmd_efibootmenu (grub_extcmd_context_t ctxt,
                       int argc __attribute__ ((unused)),
                       char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  struct bootmenu_opts o;
  void *order = NULL;
  grub_size_t order_size = 0;
  grub_uint8_t *seen;

  o.omit_inactive = state[OPT_OMIT_INACTIVE].set;
  o.savedefault = state[OPT_SAVEDEFAULT].set;
  o.category_boot_only = state[OPT_CATEGORY_BOOT_ONLY].set;

  /* Identify the running image so its own boot entry can be skipped.  */
  grub_efibootnext_self_init (&o.self);

  /* BootCurrent is only the fallback for when self-identification fails.  */
  o.current = 0;
  o.have_current = 0;
  if (!grub_efibootnext_self_known (&o.self))
    o.have_current = get_boot_uint16 ("BootCurrent", &o.current);

  /* One bit per possible Boot#### number, to skip duplicates.  */
  seen = grub_zalloc (0x10000 / 8);
  if (seen == NULL)
    {
      grub_efibootnext_self_fini (&o.self);
      return grub_errno;
    }

  /* Entries named in BootOrder, in firmware order.  */
  if (grub_efi_get_variable ("BootOrder", &global, &order_size, &order)
        == GRUB_EFI_SUCCESS && order != NULL)
    {
      grub_uint16_t *list = order;
      grub_size_t count = order_size / sizeof (grub_uint16_t);
      grub_size_t i;

      for (i = 0; i < count; i++)
        {
          /* BootOrder itself can carry duplicates; add each number once.  */
          if ((seen[list[i] >> 3] & (1 << (list[i] & 7))) != 0)
            continue;
          seen[list[i] >> 3] |= 1 << (list[i] & 7);
          add_bootnext_entry (list[i], &o);
        }
    }
  grub_free (order);

  /* Inactive entries can sit outside BootOrder, so sweep the rest too.  */
  add_remaining_entries (seen, &o);
  grub_free (seen);

  grub_efibootnext_self_fini (&o.self);
  grub_errno = GRUB_ERR_NONE;
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd_efibootmenu;

void
grub_efibootnext_menu_init (void)
{
  cmd_efibootmenu =
    grub_register_extcmd ("efibootmenu", grub_cmd_efibootmenu, 0,
                          N_("[--omit-inactive] [--savedefault] [--category-boot-only]"),
                          N_("Add a menu entry for each EFI boot option."),
                          efibootmenu_options);
}

void
grub_efibootnext_menu_fini (void)
{
  grub_unregister_extcmd (cmd_efibootmenu);
}
