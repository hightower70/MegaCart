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

#ifndef __FileUtils_h
#define __FileUtils_h

///////////////////////////////////////////////////////////////////////////////
// Includes
#include "CASFile.h"

///////////////////////////////////////////////////////////////////////////////
// Constants

// Path length
#define MAX_PATH_LENGTH 260
#define MAX_TVC_FILE_NAME_LENGTH 16

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
void ReadBlock(FILE* in_file, void* in_buffer, int in_size, bool* inout_success);
void WriteBlock(FILE* in_file, void* in_buffer, int in_size, bool* inout_success);

void GetFileNameAndExtension(wchar_t* out_file_name, int in_buffer_length, wchar_t* in_path);
void GetFileNameWithoutExtension(wchar_t* out_file_name, wchar_t* in_path);
void ChangeFileExtension(wchar_t* in_file_name, int in_buffer_length, wchar_t* in_extension);
void GetExtension(wchar_t* out_extension, wchar_t* in_path);

bool StringStartsWith(const wchar_t* in_string, const wchar_t* in_prefix);

void PCToTVCFilename(char* out_tvc_file_name, wchar_t* in_file_name);
void PCToTVCFilenameAndExtension(char* out_tvc_file_name, wchar_t* in_file_name);

bool CheckFileExists(wchar_t* in_file_name);

bool CASCheckUPMHeaderValidity(CASUPMHeaderType* in_header);
bool CASCheckHeaderValidity(CASProgramFileHeaderType* in_header);

int CompareFilenames(const wchar_t* in_filename1, const wchar_t* in_filename2);

#endif
