/*****************************************************************************/
/* MegaCartImageBuilder - Videoton TV Computer 1MByte Cart Image Builder     */
/* Image builder functions                                                   */
/*                                                                           */
/* Copyright (C) 2021-2023 Laszlo Arvai                                      */
/* All rights reserved.                                                      */
/*                                                                           */
/* This software may be modified and distributed under the terms             */
/* of the BSD license.  See the LICENSE file for details.                    */
/*****************************************************************************/

///////////////////////////////////////////////////////////////////////////////
// Includes
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <Windows.h>
#include <CASFile.h>
#include <FileUtils.h>
#include "ZX7Compress.h"

///////////////////////////////////////////////////////////////////////////////
// Constants
#define CART_PAGE_SIZE 16384					// 16 kByte
#define FILE_BUFFER_SIZE 4*1024*1024	// 4 MByte
#define MAX_FILE_NUMBER 256
#define LINE_BUFFER_SIZE 80
#define CHIN_UNCOMPRESSED_BYTE_COUNT 16	// number of characters to be read using CH_IN TVC ROM function (these bytes at the beginning of each file will not be compressed)

#define CART_TYPE_MEGACART	0
#define CART_TYPE_MULTICART	1

#define PRINT_ERROR(...) fwprintf (stderr, __VA_ARGS__)
#define PRINT_INFO(...) fwprintf (stdout, __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////
// Loader binary data
extern const long int megacart_loader_bin_size;
extern const unsigned char megacart_loader_bin[];

extern const long int megacart_decomp_loader_bin_size;
extern const unsigned char megacart_decomp_loader_bin[];

extern const long int multicart_loader_bin_size;
extern const unsigned char multicart_loader_bin[];

extern const long int multicart_decomp_loader_bin_size;
extern const unsigned char multicart_decomp_loader_bin[];

///////////////////////////////////////////////////////////////////////////////
// Types

/// <summary>
/// Information about the loaded program files
/// </summary>
typedef struct 
{
	wchar_t Filename[MAX_PATH_LENGTH];
	int BufferPos;
	int ROMAddress;
	int Length;
	bool Version2xFile;
} ProgramFileInfo;

#pragma pack(push, 1)

/// <summary>
/// Information about a file in the ROM file system
/// </summary>
typedef struct
{
	char Filename[MAX_TVC_FILE_NAME_LENGTH];
	uint16_t Address;
	uint8_t Page;
	uint16_t Length;
} ROMFileInfo;

/// <summary>
/// ROM File system information
/// </summary>
typedef struct
{
	uint8_t Files1xCount;	// Number of files in the image for 1.x TVC ROM version
	uint8_t Files2xCount;	// Number of files in the image for 2.x TVC ROM version
	uint16_t Directory1xAddress;	// Address of the directory for 1.x TVC ROM version
	uint16_t Directory2xAddress;	// Address of the directory for 1.x TVC ROM version
	uint16_t FilesAddress;				// Address of the file binary data
} ROMFileSystemInfo;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
static bool LoadFiles(void);
static bool LoadProgramFile(ProgramFileInfo* inout_cas_file);
static bool CreateROMImage(void);
static bool CreateROMLoader();
static bool CreateROMDirectory();
static bool CreateROMFileSystem();
static bool ProcessFileListEntry(wchar_t* in_file_name);
static void CopyDataToROM(int length, uint8_t* in_source);
static bool IsCASFile(ProgramFileInfo* in_file_info);

///////////////////////////////////////////////////////////////////////////////
// Global variables

uint8_t g_megacart_page_start_bytes[] = { 'M', 'O', 'P', 'S', 0xAF, 0x32, 0x00, 0xFC }; // XOR A; LD (0FC00H), A
uint8_t g_multicart_page_start_bytes[] = { 'M', 'O', 'P', 'S', 0xAF, 0x32, 0x00, 0xC0, 0x32, 0x00, 0xE0 }; // XOR A; LD (0C000H), A; LD (0E000H), A

int32_t g_cart_rom_size = 1024 * 1024; // ROM size, default is 1M

byte g_file_buffer[FILE_BUFFER_SIZE];
int g_file_buffer_length;

byte g_rom_image[FILE_BUFFER_SIZE];
int g_rom_image_address;

ProgramFileInfo g_file_info[MAX_FILE_NUMBER];
int g_file_info_count;

bool g_compressed_mode = false;

int g_rom_file_system_info_address;
int g_rom_files_address;

bool g_version_2x_enabled = false;

int g_cart_type = CART_TYPE_MEGACART;

///////////////////////////////////////////////////////////////////////////////
// Main function
int wmain(int argc, wchar_t** argv)
{
	int i;
	bool success = true;
	int file_index = 0;
	wchar_t output_file_name[MAX_PATH_LENGTH];
	FILE* output_file = NULL;

	// intro
	PRINT_INFO(L"\nROM Image Builder for 1MByte TV Computer Cartridge v1.0");
	PRINT_INFO(L"\n(c) 2021-2023 Laszlo Arvai");

	// default output file name
	wcscpy_s(output_file_name, MAX_PATH_LENGTH, L"MegaCart.bin");

	i = 1;
	while (i < argc && success)
	{
		// switch found
		if (argv[i][0] == '-')
		{
			switch (tolower(argv[i][1]))
			{
				// file list
				case 'f':
				{
					FILE* parameter_file;
					wchar_t line[LINE_BUFFER_SIZE];

					if (i + 1 < argc)
					{
						i++;
						if (_wfopen_s(&parameter_file, argv[i], L"rt") != 0)
						{
							PRINT_ERROR(L"\nCan't open filelist!");
							return false;
						}
						else
						{
							while (!feof(parameter_file))
							{
								if (fgetws(line, LINE_BUFFER_SIZE, parameter_file) != NULL)
								{
									ProcessFileListEntry(line);
								}
							}

							fclose(parameter_file);
						}
					}
					else
					{
						PRINT_ERROR(L"\nNo parameter for option 'f'.");
						success = false;
					}
				}
				break;

				// version 2.x ROM 
				case '2':
					g_version_2x_enabled = true;
					break;

				// output file name
				case 'o':
					if (i + 1 < argc)
					{
						i++;
						wcscpy_s(output_file_name, MAX_PATH_LENGTH, argv[i]);
					}
					else
					{
						PRINT_ERROR(L"\nNo parameter for option 'o'.");
						success = false;
					}
					break;

				// force compressed mode
				case 'c':
					g_compressed_mode = true;
					break;

				// sets ROM size
				case 's':
					if (i + 1 < argc)
					{
						i++;
						if (_wcsicmp(argv[i], L"512") == 0)
						{
							g_cart_rom_size = 512 * 1024;
						}
						else
						{
							if (_wcsicmp(argv[i], L"256") == 0)
							{
								g_cart_rom_size = 256 * 1024;
							}
							else
							{
								if (_wcsicmp(argv[i], L"128") == 0)
								{
									g_cart_rom_size = 128 * 1024;
								}
								else
								{
									PRINT_ERROR(L"\nInvalid ROM size.");
									success = false;
								}
							}
						}
					}
					else
					{
						PRINT_ERROR(L"\nNo parameter for option 's'.");
						success = false;
					}
					break;

				// cart type
				case 't':
					if (i + 1 < argc)
					{
						i++;
						if (_wcsicmp(argv[i], L"0") == 0)
						{
							g_cart_type = CART_TYPE_MEGACART;
						}
						else
						{
							if (_wcsicmp(argv[i], L"1") == 0)
							{
								g_cart_type = CART_TYPE_MULTICART;
							}
							else
							{
								PRINT_ERROR(L"\nInvalid cart type.");
								success = false;
							}
						}
					}
					else
					{
						PRINT_ERROR(L"\nNo parameter for option 't'.");
						success = false;
					}
					break;


				// help text
				case 'h':
				case'?':
					PRINT_INFO(L"\nUsage: MegaCartImageBuilder.exe startup.cas file1.cas file2.cas\n");
					PRINT_INFO(L" Creates ROM image which autostarts with 'startup.cas'. All other files are available by using LOAD instruction from\n");
					PRINT_INFO(L" any TVC program. Number of files are limited only the available space.\n");
					PRINT_INFO(L"\n Options:\n");
					PRINT_INFO(L" -o: sets the output (ROM image) file name. The default is 'MegaCart.bin'.\n");
					PRINT_INFO(L"     example: '-o cartridge.bin'option sets the output file name to 'cartridge.bin'.\n");
					PRINT_INFO(L" -2: Switches to ROM version 2.x file system. All files are specified after this option\n");
					PRINT_INFO(L"     will be handled as 2.x files.\n");
					PRINT_INFO(L"     example: MegaCartImageBuilder.exe startup_v1.cas file_v1 -2 startup_v2.cas file_v2\n");
					PRINT_INFO(L"     If same file name is intended to be used for both file system then it is recommended\n");
					PRINT_INFO(L"     to put them into separated folder and specifiy their path in the filename.\n");
					PRINT_INFO(L"     The path will not be stored in the ROM image.\n");
					PRINT_INFO(L" -c: Forces to compressed ROM image. The data content will be compressed by ZX7 compressor\n");
					PRINT_INFO(L"     and will be decompressed on the fly when the file is loaded. If compression if not forced\n");
					PRINT_INFO(L"     the image builder will switch only to comressed mode when the specified files can't fit to the ROM.\n");
					PRINT_INFO(L" -f: Uses text file instead of the list of files in the command line. All lines in the specified text file\n");
					PRINT_INFO(L"     is used as a file name entry.\n");
					PRINT_INFO(L"     example: '-f filename.txt' option will use 'filename.txt' file to collect file names to be included\n");
					PRINT_INFO(L"     on the cart. This option can be mixed with other options: '-f filename1.txt -2 -f filename2.txt'\n");
					PRINT_INFO(L"     'filename1.txt specifies the file names for ROM 1.x version while filename2.txt specifies file names\n");
					PRINT_INFO(L"     for ROM 2.x version.\n");
					PRINT_INFO(L" -s: Sets ROM size. The default size is 1Mbyte. The size can be set to 512kB, 256kB or 128kB\n");
					PRINT_INFO(L"     '-s 512' sets 512kB ROM, '-s 256' sets 256kB, '-s 128' sets 128kB ROM size\n");
					PRINT_INFO(L" -t: Sets target card type. (0 - megacart (default), 1 - multicart)\n");
					PRINT_INFO(L"     '-t 1' selects the multicart\n");
					success = false;
					break;
			}
		}
		else
		{
			// filename found
			success = ProcessFileListEntry(argv[i]);
		}

		i++;
	}

	// print mode
	if(success)
	{
		// copy page start bytes
		switch (g_cart_type)
		{
			case CART_TYPE_MEGACART:
				PRINT_INFO(L"\nMegaCart Mode.\n");
				break;

			case CART_TYPE_MULTICART:
				PRINT_INFO(L"\nMultiCart Mode.\n");
				break;
		}
	}

	// Loads CAS files
	if (success)
	{
		g_file_buffer_length = 0;
		success = LoadFiles();
	}

	// Creates ROM image
	if (success)
	{
		success = CreateROMImage();
	}

	// saves ROM image
	if (success)
	{
		if (_wfopen_s(&output_file, output_file_name, L"wb") == 0 && output_file != NULL)
		{
			fwrite(g_rom_image, g_cart_rom_size, 1, output_file);
			fclose(output_file);
		}
	}

	return (success) ? 0 : -1;
}


///////////////////////////////////////////////////////////////////////////////
// Processes one command line option
static bool ProcessFileListEntry(wchar_t* in_file_name)
{
	bool success = true;
	int i, j;

	// trim filename
	i = wcslen(in_file_name);
	if (i > 0)
	{
		i--;
		while (iswspace(in_file_name[i]))
			in_file_name[i--] = '\0';
	}

	i = 0;
	while (iswspace(in_file_name[i]))
	{
		i++;
	}

	j = 0;
	while (in_file_name[i] != '\0')
	{
		in_file_name[j] = in_file_name[i];
		i++;
		j++;
	}
	in_file_name[j] = '\0';

	// check for empty file
	if (wcslen(in_file_name) == 0)
		return true;

	// filename found
	wcsncpy_s(g_file_info[g_file_info_count].Filename, MAX_PATH_LENGTH, in_file_name, MAX_PATH_LENGTH);
	g_file_info[g_file_info_count].Version2xFile = g_version_2x_enabled;
	g_file_info_count++;

	return success;
}

///////////////////////////////////////////////////////////////////////////////
// Loads all CAS files
static bool LoadFiles()
{
	int i;
	bool success = true;

	for (i = 0; i < g_file_info_count && success; i++)
	{
		if (i > 0 && g_file_info[i].Version2xFile && !g_file_info[i - 1].Version2xFile)
			PRINT_INFO(L"\n*** Loading ROM 2.x files ***");

		success = LoadProgramFile(&g_file_info[i]);
	}

	return success;
}

///////////////////////////////////////////////////////////////////////////////
// Load program file
static bool LoadProgramFile(ProgramFileInfo* inout_program_file)
{
	FILE* program_file = NULL;
	bool success = true;
	CASUPMHeaderType upm_header;
	CASProgramFileHeaderType program_header;
	wchar_t display_filename[MAX_PATH_LENGTH];
	wchar_t file_extension[MAX_PATH_LENGTH];
	bool cas_file_type = true;

	// convert and copy file name
	GetFileNameAndExtension(display_filename, MAX_PATH_LENGTH, inout_program_file->Filename);
	PRINT_INFO(L"\nLoading: %s", display_filename);

	// determine file type by extension
	GetExtension(file_extension, display_filename);
	if (_wcsicmp(file_extension, L"CAS") != 0)
		cas_file_type = false;

	// open program file
	if (_wfopen_s(&program_file, inout_program_file->Filename, L"rb") != 0 || program_file == NULL)
	{
		PRINT_ERROR(L"\nCan't open file!");
		return false;
	}

	if (cas_file_type)
	{
		// load UPM header
		ReadBlock(program_file, &upm_header, sizeof(upm_header), &success);

		// load program header
		ReadBlock(program_file, &program_header, sizeof(program_header), &success);

		// Check validity
		if (!CASCheckHeaderValidity(&program_header))
		{
			PRINT_ERROR(L"\nInvalid file!");
			success = false;
		}

		if (!CASCheckUPMHeaderValidity(&upm_header))
		{
			PRINT_ERROR(L"\nInvalid file!");
			success = false;
		}
	}
	else
	{
		// determine length
		fseek(program_file, 0, SEEK_END);

		program_header.FileLength = (uint16_t)ftell(program_file);
		fseek(program_file, 0, SEEK_SET);
	}

	// check size
	if (success)
	{
		if (g_file_buffer_length + program_header.FileLength >= FILE_BUFFER_SIZE)
		{
			PRINT_ERROR(L"\nToo many file specified!");
			success = false;
		}
	}

	// load program data
	if (success)
	{
		ReadBlock(program_file, g_file_buffer + g_file_buffer_length, program_header.FileLength, &success);

		if (success)
		{
			inout_program_file->Length = program_header.FileLength;
			inout_program_file->BufferPos = g_file_buffer_length;
			g_file_buffer_length += program_header.FileLength;
			inout_program_file->ROMAddress = 0;
		}
		else
		{
			PRINT_ERROR(L"\nFile load error!");
			success = false;
		}
	}
	
	// close file
	if (program_file != NULL)
		fclose(program_file);

	return success;
}

///////////////////////////////////////////////////////////////////////////////
// Creates ROM image
static bool CreateROMImage(void)
{
	bool success = true;

	do
	{
		g_rom_image_address = 0;

		// load loader code
		if (success)
			success = CreateROMLoader();

		if (success)
		{
			// update addresses
			g_rom_file_system_info_address = g_rom_image_address - sizeof(ROMFileSystemInfo);
			g_rom_files_address = g_rom_file_system_info_address + sizeof(ROMFileSystemInfo) + sizeof(ROMFileInfo) * g_file_info_count;
			g_rom_image_address = g_rom_files_address;

			if (g_compressed_mode)
				PRINT_INFO(L"\nBuilding Compressed ROM file system.");
			else
				PRINT_INFO(L"\nBuilding ROM file system.");

			success = CreateROMFileSystem();
		}

		// check if image is fit into the ROM
		if (success && g_rom_image_address >= g_cart_rom_size)
		{
			if (g_compressed_mode)
			{
				PRINT_ERROR(L"\nCartridge memory is too low!");
				success = false;
			}
			else
			{
				// try compressed mode
				g_compressed_mode = true;
			}
		}
		else
		{
			// add directory to the image 
			if (success)
				success = CreateROMDirectory();
		}

	} while (success && g_rom_image_address >= g_cart_rom_size);

	// display statistics
	if (g_compressed_mode)
		PRINT_INFO(L"\nCompressed mode statistic:");
	else
		PRINT_INFO(L"\nStorage statistics:");

	PRINT_INFO(L" %d bytes used, %d bytes free, %d total bytes (%dkB)", g_rom_image_address, g_cart_rom_size - g_rom_image_address, g_cart_rom_size, g_cart_rom_size / 1024);

	// fill remaining bytes with FFH
	if (success)
	{
		while (g_rom_image_address < g_cart_rom_size)
		{
			if ((g_rom_image_address % CART_PAGE_SIZE) == 0)
			{
				// copy page start bytes
				switch (g_cart_type)
				{
					case CART_TYPE_MEGACART:
						memcpy(g_rom_image + g_rom_image_address, g_megacart_page_start_bytes, sizeof(g_megacart_page_start_bytes));
						g_rom_image_address += sizeof(g_megacart_page_start_bytes);
						break;

					case CART_TYPE_MULTICART:
						memcpy(g_rom_image + g_rom_image_address, g_multicart_page_start_bytes, sizeof(g_multicart_page_start_bytes));
						g_rom_image_address += sizeof(g_multicart_page_start_bytes);
						break;
				}
			}

			g_rom_image[g_rom_image_address++] = 0xff;
		}
	}

	return success;
}

///////////////////////////////////////////////////////////////////////////////
// Creates loader code
static bool CreateROMLoader()
{
	unsigned const char* loader;
	int loader_length;

	switch (g_cart_type)
	{
		case CART_TYPE_MEGACART:
			if (g_compressed_mode)
			{
				loader = megacart_decomp_loader_bin;
				loader_length = megacart_decomp_loader_bin_size;
			}
			else
			{
				loader = megacart_loader_bin;
				loader_length = megacart_loader_bin_size;
			}
			break;

		case CART_TYPE_MULTICART:
			if (g_compressed_mode)
			{
				loader = multicart_decomp_loader_bin;
				loader_length = multicart_decomp_loader_bin_size;
			}
			else
			{
				loader = multicart_loader_bin;
				loader_length = multicart_loader_bin_size;
			}
			break;

		default:
			return false;
	}

	// copy loader to ROM image
	memcpy(g_rom_image, loader, loader_length);

	// update ROM address
	g_rom_image_address = loader_length;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// Creates directory on the ROM image
static bool CreateROMDirectory()
{
	ROMFileInfo* file_info;
	int file_info_address;
	int file_count = 0;
	bool file_system_version2x = false;
	wchar_t buffer[MAX_PATH_LENGTH];

	ROMFileSystemInfo* file_system_info = (ROMFileSystemInfo*)(g_rom_image + g_rom_file_system_info_address);
	file_system_info->FilesAddress = g_rom_files_address;
	file_system_info->Directory1xAddress = g_rom_file_system_info_address + sizeof(ROMFileSystemInfo);

	// create directory entries
	for (int i = 0; i < g_file_info_count; i++)
	{
		file_info_address = g_rom_file_system_info_address + sizeof(ROMFileSystemInfo) + i * sizeof(ROMFileInfo);
		file_info = (ROMFileInfo*)(g_rom_image + file_info_address);

		// change to 2x ROM version if required
		if (g_file_info[i].Version2xFile && !file_system_version2x)
		{
			file_system_version2x = true;
			file_system_info->Files1xCount = (uint8_t)file_count;
			file_count = 0;
			file_system_info->Directory2xAddress = (uint16_t)file_info_address;
		}

		// convert and copy file name
		GetFileNameAndExtension(buffer, MAX_PATH_LENGTH, g_file_info[i].Filename);
		_wcsupr_s(buffer, MAX_PATH_LENGTH);
		PCToTVCFilenameAndExtension(file_info->Filename, buffer);

		file_info->Address = (g_file_info[i].ROMAddress %CART_PAGE_SIZE);
		file_info->Page = (g_file_info[i].ROMAddress / CART_PAGE_SIZE);
		file_info->Length = (uint16_t)g_file_info[i].Length;

		file_count++;
	}

	// update file system info
	if (file_system_version2x)
	{
		file_system_info->Files2xCount = file_count;
	}
	else
	{
		file_system_info->Files1xCount = file_count;
		file_system_info->Directory2xAddress = file_system_info->Directory1xAddress;
		file_system_info->Files2xCount = file_system_info->Files1xCount;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// Creates files on the ROM image
static bool CreateROMFileSystem()
{
	int j;
	uint8_t* compressed_data = NULL;
	size_t compressed_size = 0;
	int length;
	uint8_t* source;
	bool multiple_file;

	// generate files in the ROM
	for (int i = 0; i < g_file_info_count; i++)
	{
		// check if file is already in the ROM image
		multiple_file = false;
		j = 0;
		while (j < i)
		{
			// compare length
			if (g_file_info[i].Length == g_file_info[j].Length)
			{
				uint8_t* ipos = &g_file_buffer[g_file_info[i].BufferPos];
				uint8_t* jpos = &g_file_buffer[g_file_info[j].BufferPos];

				// compare content if length is same
				multiple_file = true;
				for (int pos = 0; pos < g_file_info[i].Length; pos++)
				{
					if (*ipos != *jpos)
					{
						multiple_file = false;
						break;
					}

					ipos++;
					jpos++;
				}
			}

			if (multiple_file)
			{
				break;
			}
			else
			{
				j++;
			}
		}

		if (multiple_file)
		{
			// file already included in the image, copy only the address
			g_file_info[i].ROMAddress = g_file_info[j].ROMAddress;
		}
		else
		{
			// update ROM address
			g_file_info[i].ROMAddress = g_rom_image_address;

			if (g_compressed_mode)
			{
				if (IsCASFile(&g_file_info[i]))
				{
					compressed_data = ZX7Compress(ZX7Optimize(g_file_buffer + g_file_info[i].BufferPos, g_file_info[i].Length), g_file_buffer + g_file_info[i].BufferPos, g_file_info[i].Length, &compressed_size);
					length = (int)compressed_size;
					source = compressed_data;
				}
				else
				{
					// compression mode
					if (g_file_info[i].Length > CHIN_UNCOMPRESSED_BYTE_COUNT)
					{
						// copy first bytes of each file (without compression)
						CopyDataToROM(CHIN_UNCOMPRESSED_BYTE_COUNT, (uint8_t*)(g_file_buffer + g_file_info[i].BufferPos));

						// copy remaining bytes using compression
						compressed_data = ZX7Compress(ZX7Optimize(g_file_buffer + g_file_info[i].BufferPos + CHIN_UNCOMPRESSED_BYTE_COUNT, g_file_info[i].Length - CHIN_UNCOMPRESSED_BYTE_COUNT), g_file_buffer + g_file_info[i].BufferPos + CHIN_UNCOMPRESSED_BYTE_COUNT, g_file_info[i].Length - CHIN_UNCOMPRESSED_BYTE_COUNT, &compressed_size);
						length = (int)compressed_size;
						source = compressed_data;
					}
					else
					{
						// store only since the file length is smaller than CHIN_BYTE_COUNT
						source = (uint8_t*)(g_file_buffer + g_file_info[i].BufferPos);
						length = g_file_info[i].Length;
					}
				}
			}
			else
			{
				// store mode (no compression)
				source = (uint8_t*)(g_file_buffer + g_file_info[i].BufferPos);
				length = g_file_info[i].Length;
			}

			// copy file to the ROM image
			CopyDataToROM(length, source);

			if(g_compressed_mode)
			{
				free(compressed_data);
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// Copies data from the file buffer to the ROM buffer
static void CopyDataToROM(int length, uint8_t* in_source)
{
	int byte_count;

	for (byte_count = 0; byte_count < length; byte_count++)
	{
		// check for page start
		if ((g_rom_image_address % CART_PAGE_SIZE) == 0)
		{
			// copy page start bytes
			switch (g_cart_type)
			{
				case CART_TYPE_MEGACART:
					memcpy(g_rom_image + g_rom_image_address, g_megacart_page_start_bytes, sizeof(g_megacart_page_start_bytes));
					g_rom_image_address += sizeof(g_megacart_page_start_bytes);
					break;

				case CART_TYPE_MULTICART:
					memcpy(g_rom_image + g_rom_image_address, g_multicart_page_start_bytes, sizeof(g_multicart_page_start_bytes));
					g_rom_image_address += sizeof(g_multicart_page_start_bytes);
					break;
			}
		}

		g_rom_image[g_rom_image_address++] = *in_source;
		in_source++;
	}
}

static bool IsCASFile(ProgramFileInfo* in_file_info)
{
	wchar_t* dot_pos = wcsrchr(in_file_info->Filename, L'.');

	if (dot_pos != NULL)
	{
		return _wcsicmp(dot_pos, L".CAS") == 0;
	}

	return false;
}