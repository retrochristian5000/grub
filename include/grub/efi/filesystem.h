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

#ifndef GRUB_EFI_FILESYSTEM_HEADER
#define GRUB_EFI_FILESYSTEM_HEADER 1

#include <grub/efi/api.h>
#include <grub/env.h>

struct grub_efi_file_io_token {
  grub_efi_event_t Event;
  grub_efi_status_t Status;
  grub_efi_uint64_t BufferSize;
  void *Buffer;
};

typedef struct grub_efi_file_io_token grub_efi_file_io_token_t;


struct grub_efi_file_protocol
{
  grub_efi_uint64_t Revision;

  grub_efi_status_t
  (__grub_efi_api *Open) (struct grub_efi_file_protocol *this,
		  struct grub_efi_file_protocol **new_handle,
		  grub_efi_char16_t *filename,
		  grub_efi_uint64_t open_mode,
		  grub_efi_uint64_t attributes);

  grub_efi_status_t
  (__grub_efi_api *Close) (struct grub_efi_file_protocol *this);

  grub_efi_status_t
  (__grub_efi_api *Delete) (struct grub_efi_file_protocol *this);

  grub_efi_status_t
  (__grub_efi_api *Read) (struct grub_efi_file_protocol *this,
		  grub_efi_uint64_t *buffer_size,
		  void *buffer);

  grub_efi_status_t
  (__grub_efi_api *Write) (struct grub_efi_file_protocol *this,
		  grub_efi_uint64_t *buffer_size,
		  void *buffer);

  grub_efi_status_t
  (__grub_efi_api *GetPosition) (struct grub_efi_file_protocol *this,
		  grub_efi_uint64_t *position);

  grub_efi_status_t
  (__grub_efi_api *SetPosition) (struct grub_efi_file_protocol *this,
		  grub_efi_uint64_t position);

  grub_efi_status_t
  (__grub_efi_api *GetInfo) (struct grub_efi_file_protocol *this,
		  grub_guid_t *information_type,
		  grub_efi_uint64_t *buffer_size,
		  void *buffer);

  grub_efi_status_t
  (__grub_efi_api *SetInfo) (struct grub_efi_file_protocol *this,
		  grub_guid_t *information_type,
		  grub_efi_uint64_t buffer_size,
		  void *buffer);

  grub_efi_status_t
  (__grub_efi_api *Flush) (struct grub_efi_file_protocol *this);

  grub_efi_status_t
  (__grub_efi_api *OpenEx) (struct grub_efi_file_protocol *this,
		  struct grub_efi_file_protocol **new_handle,
		  grub_efi_char16_t *filename,
		  grub_efi_uint64_t open_mode,
		  grub_efi_uint64_t attributes,
		  grub_efi_file_io_token_t *token);

  grub_efi_status_t
  (__grub_efi_api *ReadEx) (struct grub_efi_file_protocol *this,
		  grub_efi_file_io_token_t *token);

  grub_efi_status_t
  (__grub_efi_api *WriteEx) (struct grub_efi_file_protocol *this,
		  grub_efi_file_io_token_t *token);

  grub_efi_status_t
  (__grub_efi_api *FlushEx) (struct grub_efi_file_protocol *this,
		  grub_efi_file_io_token_t *token);
};

typedef struct grub_efi_file_protocol grub_efi_file_protocol_t;

/*******************************************************
  Open Modes
 ******************************************************/
#define GRUB_EFI_FILE_MODE_READ       0x0000000000000001
#define GRUB_EFI_FILE_MODE_WRITE      0x0000000000000002
#define GRUB_EFI_FILE_MODE_CREATE     0x8000000000000000

/*******************************************************
  File Attributes
 ******************************************************/
#define GRUB_EFI_FILE_READ_ONLY       0x0000000000000001
#define GRUB_EFI_FILE_HIDDEN	      0x0000000000000002
#define GRUB_EFI_FILE_SYSTEM	      0x0000000000000004
#define GRUB_EFI_FILE_RESERVED	      0x0000000000000008
#define GRUB_EFI_FILE_DIRECTORY       0x0000000000000010
#define GRUB_EFI_FILE_ARCHIVE	      0x0000000000000020
#define GRUB_EFI_FILE_VALID_ATTR      0x0000000000000037

struct grub_efi_file_info {
  grub_efi_uint64_t Size;
  grub_efi_uint64_t FileSize;
  grub_efi_uint64_t PhysicalSize;
  grub_efi_time_t CreateTime;
  grub_efi_time_t LastAccessTime;
  grub_efi_time_t ModificationTime;
  grub_efi_uint64_t Attribute;
  grub_efi_char16_t FileName[];
};

typedef struct grub_efi_file_info grub_efi_file_info_t;

#define GRUB_EFI_FILE_INFO_ID \
 {0x09576e92,0x6d3f,0x11d2, \
    {0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b} \
 }

struct grub_efi_simple_file_system_protocol
{
  grub_efi_uint64_t Revision;

  grub_efi_status_t
  (__grub_efi_api *OpenVolume) (struct grub_efi_simple_file_system_protocol *this,
				struct grub_efi_file_protocol **root);
};

typedef struct grub_efi_simple_file_system_protocol grub_efi_simple_file_system_protocol_t;

#endif
