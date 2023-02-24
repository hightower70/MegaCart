/*****************************************************************************/
/* MegaCartImageBuilder - Videoton TV Computer 1MByte Cart Image Builder     */
/* File handling helper functions                                            */
/*                                                                           */
/* Copyright (C) 2021-2023 Laszlo Arvai                                      */
/* All rights reserved.                                                      */
/*                                                                           */
/* This software may be modified and distributed under the terms             */
/* of the BSD license.  See the LICENSE file for details.                    */
/*****************************************************************************/

///////////////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "CharMap.h"
#include "FileUtils.h"


///////////////////////////////////////////////////////////////////////////////
// Types

///////////////////////////////////////////////////////////////////////////////
// Module global variables

///////////////////////////////////////////////////////////////////////////////
// Checks if file exists
bool CheckFileExists(wchar_t* in_file_name)
{
	FILE* test;

	if (_wfopen_s(&test, in_file_name, L"r") == 0)
	{
		if (test != NULL)
			fclose(test);

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Changes file name extension
void ChangeFileExtension(wchar_t* in_file_name, int in_buffer_length, wchar_t* in_extension)
{
	wchar_t* extension;

	extension = wcsrchr(in_file_name, '.');

	if(extension != NULL)
	{
		extension++;
		wcscpy_s(extension, in_buffer_length, in_extension);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Gets filename and extension from full file name
void GetFileNameAndExtension(wchar_t* out_file_name, int in_buffer_length, wchar_t* in_path)
{
	wchar_t* filename;

	filename = wcsrchr(in_path, '\\');

	if(filename==NULL)
		filename = wcsrchr(in_path, '/'); // Accept either \ or /

	if(filename != NULL)
	{
		filename++;
	}
	else
	{
		filename = in_path;
	}

	wcscpy_s(out_file_name, in_buffer_length, filename);
}

///////////////////////////////////////////////////////////////////////////////
// Gets filename without extension from full file name
void GetFileNameWithoutExtension(wchar_t* out_file_name, wchar_t* in_path)
{
	wchar_t* filename_start;
	wchar_t* filename_end;

	// determine start of the filename
	filename_start = wcsrchr(in_path, '\\');

	if(filename_start != NULL)
	{
		filename_start++;
	}
	else
	{
		filename_start = in_path;
	}

	// determine end of the filename
	filename_end = wcsrchr(in_path, '.');
	
	// copy filename
	while( *filename_start != '\0' && (filename_end == NULL || filename_start < filename_end))
	{
		*out_file_name++ = *filename_start++;
	}
	*out_file_name = '\0';
}

///////////////////////////////////////////////////////////////////////////////
// Gets extension from full file name
void GetExtension(wchar_t* out_extension, wchar_t* in_path)
{
	wchar_t* extension_pos;

	*out_extension = '\0';

	// determine end of the filename
	extension_pos = wcsrchr(in_path, '.');
	if (extension_pos != NULL)
	{
		extension_pos++; // skip .
	
		// copy extension
		wcscpy_s(out_extension, MAX_PATH_LENGTH, extension_pos);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Convert PC filename to TVC filename
void PCToTVCFilename(char* out_tvc_file_name, wchar_t* in_file_name)
{
	wchar_t buffer[MAX_PATH_LENGTH];

	// get filename only
	GetFileNameWithoutExtension(buffer, in_file_name);

	// limit length
	if (wcslen(buffer) > MAX_TVC_FILE_NAME_LENGTH)
		buffer[MAX_TVC_FILE_NAME_LENGTH] = '\0';

	// convert charmap
	UNICODEStringToTVCString(out_tvc_file_name, buffer);
}

///////////////////////////////////////////////////////////////////////////////
// Convert PC filename to TVC filename
void PCToTVCFilenameAndExtension(char* out_tvc_file_name, wchar_t* in_file_name)
{
	wchar_t buffer[MAX_PATH_LENGTH];

	// get filename only
	GetFileNameAndExtension(buffer, MAX_PATH_LENGTH, in_file_name);

	// limit length
	if (wcslen(buffer) > MAX_TVC_FILE_NAME_LENGTH)
		buffer[MAX_TVC_FILE_NAME_LENGTH] = '\0';

	// convert charmap
	UNICODEStringToTVCString(out_tvc_file_name, buffer);
}

///////////////////////////////////////////////////////////////////////////////
// Generates TVC filename from file full path 
void TVCToPCFilename(wchar_t* out_tvc_file_name, char* in_file_name)
{
	int source_index, destintion_index;
	wchar_t ch;

	// handle empty file name
	if(in_file_name[0] == '\0')
	{
		in_file_name = "tvcdefault";
	}

	// convert to unicode and remove invalid path characters
	source_index = 0;
	destintion_index = 0;
	while(source_index < MAX_PATH_LENGTH && destintion_index<MAX_PATH_LENGTH-1 && in_file_name[source_index] != '\0')
	{
		ch = TVCCharToUNICODEChar(in_file_name[source_index++]);

		// skip invalid characters for file name
		if(ch != '<' && ch != '>' && ch != ':' && ch != '"' && ch !='/' && ch != '\\' && ch != '|' && ch != '?' && ch != '*')
			out_tvc_file_name[destintion_index++] = ch;
	}

	out_tvc_file_name[destintion_index] = '\0';
}

///////////////////////////////////////////////////////////////////////////////
// String starts with comparision
bool StringStartsWith(const wchar_t* in_string, const wchar_t* in_prefix)
{
	size_t string_length = wcslen(in_string);
	size_t prefix_length = wcslen(in_prefix);

  return string_length < prefix_length ? false : _wcsnicmp(in_string, in_prefix, prefix_length) == 0;
}


///////////////////////////////////////////////////////////////////////////////
// Reads a block from a file and sets success flag
void ReadBlock(FILE* in_file, void* in_buffer, int in_size, bool* inout_success)
{
	if(!(*inout_success))
		return;

	if(fread(in_buffer, in_size, 1, in_file) != 1)
		*inout_success = false;
}

///////////////////////////////////////////////////////////////////////////////
// Writes a block to the file and sets success flag
void WriteBlock(FILE* in_file, void* in_buffer, int in_size, bool* inout_success)
{
	if(!(*inout_success))
		return;

	if(in_size==0)
		return;

	*inout_success = (fwrite(in_buffer, in_size, 1, in_file) == 1);
}

///////////////////////////////////////////////////////////////////////////////
// Checks UPM header validity
bool CASCheckUPMHeaderValidity(CASUPMHeaderType* in_header)
{
	if (in_header->FileType == CASBLOCKHDR_FILE_UNBUFFERED)
		return true;
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
// Cheks CAS header validity
bool CASCheckHeaderValidity(CASProgramFileHeaderType* in_header)
{
	if (in_header->Zero == 0 &&
		(in_header->FileType == UPMPROGTYPE_PRG ||
			in_header->FileType == UPMPROGTYPE_ASCII))
		return true;
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
// Compares filenames
int CompareFilenames(const wchar_t* in_filename1, const wchar_t* in_filename2)
{
	return _wcsicmp(in_filename1, in_filename2);
}