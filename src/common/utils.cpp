// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "utils.hpp"

#include <cmath> // floor()
#include <cstdlib>
#include <cstring>

#ifdef WIN32
	#include "winapi.hpp"
	#ifndef F_OK
		#define F_OK   0x0
	#endif  /* F_OK */
#else
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/stat.h>
#endif

#include "byteorder.hpp"
#include "cbasetypes.hpp"
#include "showmsg.hpp"
#include "socket.hpp"
#include "strlib.hpp"

/// Dumps given buffer into file pointed to by a handle.
void WriteDump(FILE* fp, const void* buffer, size_t length)
{
	size_t i;
	char hex[48+1], ascii[16+1];

	fprintf(fp, "--- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F   0123456789ABCDEF\n");
	ascii[16] = 0;

	for( i = 0; i < length; i++ )
	{
		char c = RBUFB(buffer,i);

		ascii[i%16] = ISCNTRL(c) ? '.' : c;
		sprintf(hex+(i%16)*3, "%02X ", RBUFB(buffer,i));

		if( (i%16) == 15 )
		{
			fprintf(fp, "%03X %s  %s\n", (uint32)(i/16), hex, ascii);
		}
	}

	if( (i%16) != 0 )
	{
		ascii[i%16] = 0;
		fprintf(fp, "%03X %-48s  %-16s\n", (uint32)(i/16), hex, ascii);
	}
}


/// Dumps given buffer on the console.
void ShowDump(const void* buffer, size_t length)
{
	size_t i;
	char hex[48+1], ascii[16+1];

	ShowDebug("--- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F   0123456789ABCDEF\n");
	ascii[16] = 0;
        
	for( i = 0; i < length; i++ )
	{
		char c = RBUFB(buffer,i);

		ascii[i%16] = ISCNTRL(c) ? '.' : c;
		sprintf(hex+(i%16)*3, "%02X ", RBUFB(buffer,i));

		if( (i%16) == 15 )
		{
			ShowDebug("%03" PRIXPTR " %s  %s\n", i/16, hex, ascii);
		}
	}

	if( (i%16) != 0 )
	{
		ascii[i%16] = 0;
		ShowDebug("%03" PRIXPTR " %-48s  %-16s\n", i/16, hex, ascii);
	}
}


#ifdef WIN32

static char* checkpath(char *path, size_t size, const char *srcpath)
{	// just make sure the char*path is not const
	char *p=path;
	if(nullptr!=path && nullptr!=srcpath)
	while(*srcpath && (size_t)(p - path) < size - 1) {
		if (*srcpath=='/') {
			*p++ = '\\';
			srcpath++;
		}
		else
			*p++ = *srcpath++;
	}
	*p = '\0'; //EOS
	return path;
}

void findfile(const char *p, const char *pat, void (func)(const char*))
{
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind;
	char tmppath[MAX_PATH+1];

	const char *path    = (p  ==nullptr)? "." : p;
	const char *pattern = (pat==nullptr)? "" : pat;

	checkpath(tmppath, sizeof(tmppath), path);
	if( PATHSEP != tmppath[strlen(tmppath)-1])
		strncat(tmppath, "\\*", sizeof(tmppath) - strlen(tmppath) - 1);
	else
		strncat(tmppath, "*", sizeof(tmppath) - strlen(tmppath) - 1);

	hFind = FindFirstFileA(tmppath, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (strcmp(FindFileData.cFileName, ".") == 0)
				continue;
			if (strcmp(FindFileData.cFileName, "..") == 0)
				continue;

			snprintf(tmppath, sizeof(tmppath), "%s%c%s", path, PATHSEP, FindFileData.cFileName);

			if (FindFileData.cFileName && strstr(FindFileData.cFileName, pattern)) {
				func( tmppath );
			}


			if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				findfile(tmppath, pat, func);
			}
		}while (FindNextFileA(hFind, &FindFileData) != 0);
		FindClose(hFind);
	}
	return;
}

/**
 * Check if the path is a directory or file
 * @param filepath: Location of file
 * @return 0 = Error
 *         1 = Directory
 *         2 = File
 *         3 = File but doesn't exist
 */
int32 check_filepath(const char* filepath)
{
	DWORD Attribute;

	if (Attribute = GetFileAttributes(filepath)) {
		if ((Attribute&INVALID_FILE_ATTRIBUTES) && GetLastError() == ERROR_FILE_NOT_FOUND) {
			SetLastError(0);
			return 3;
		} else if (Attribute&FILE_ATTRIBUTE_DIRECTORY)
			return 1;
		else
			return 2;
	}

	return 0;
}

#else

#define MAX_DIR_PATH 2048

/**
 * Check if the path is a directory or file
 * @param filepath: Location of file
 * @return 0 = Error
 *         1 = Directory
 *         2 = File
 *         3 = Neither a file or directory
 */
int32 check_filepath(const char* filepath)
{
	struct stat s;

	if (stat(filepath, &s) == 0) {
		if (s.st_mode&S_IFDIR)
			return 1;
		else if (s.st_mode&S_IFREG)
			return 2;
		else
			return 3;
	}

	return 0;
}

static char* checkpath(char *path, size_t size, const char*srcpath)
{	// just make sure the char*path is not const
	char *p=path;
	if(nullptr!=path && nullptr!=srcpath)
	while(*srcpath && (size_t)(p - path) < size - 1) {
		if (*srcpath=='\\') {
			*p++ = '/';
			srcpath++;
		}
		else
			*p++ = *srcpath++;
	}
	*p = '\0'; //EOS
	return path;
}

void findfile(const char *p, const char *pat, void (func)(const char*))
{
	DIR* dir;					// pointer to the scanned directory.
	struct dirent* entry;		// pointer to one directory entry.
	struct stat dir_stat;       // used by stat().
	char tmppath[MAX_DIR_PATH * 2];
	char path[MAX_DIR_PATH+1]= ".";
	const char *pattern = (pat==nullptr)? "" : pat;
	if(p!=nullptr) safestrncpy(path, p, sizeof(path));

	// open the directory for reading
	dir = opendir( checkpath(path, sizeof(path), path) );
	if (!dir) {
		ShowError("Cannot read directory '%s'\n", path);
		return;
	}

	// scan the directory, traversing each sub-directory
	// matching the pattern for each file name.
	while ((entry = readdir(dir))) {
		// skip the "." and ".." entries.
		if (strcmp(entry->d_name, ".") == 0)
			continue;
		if (strcmp(entry->d_name, "..") == 0)
			continue;

		sprintf(tmppath,"%s%c%s",path, PATHSEP, entry->d_name);

		// check if the pattern matchs.
		if (entry->d_name[0] && strstr(entry->d_name, pattern)) {
			func( tmppath );
		}
		// check if it is a directory.
		if (stat(tmppath, &dir_stat) == -1) {
			ShowError("stat error %s\n': ", tmppath);
			continue;
		}
		// is this a directory?
		if (S_ISDIR(dir_stat.st_mode)) {
			// decent recursivly
			findfile(tmppath, pat, func);
		}
	}//end while

	closedir(dir);
}
#endif

bool exists(const char* filename)
{
	return !access(filename, F_OK);
}

uint8 GetByte(uint32 val, int32 idx)
{
	switch( idx )
	{
	case 0: return (uint8)( (val & 0x000000FF)         );
	case 1: return (uint8)( (val & 0x0000FF00) >> 0x08 );
	case 2: return (uint8)( (val & 0x00FF0000) >> 0x10 );
	case 3: return (uint8)( (val & 0xFF000000) >> 0x18 );
	default:
#if defined(DEBUG)
		ShowDebug("GetByte: invalid index (idx=%d)\n", idx);
#endif
		return 0;
	}
}

uint16 GetWord(uint32 val, int32 idx)
{
	switch( idx )
	{
	case 0: return (uint16)( (val & 0x0000FFFF)         );
	case 1: return (uint16)( (val & 0xFFFF0000) >> 0x10 );
	default:
#if defined(DEBUG)
		ShowDebug("GetWord: invalid index (idx=%d)\n", idx);
#endif
		return 0;
	}
}
uint16 MakeWord(uint8 byte0, uint8 byte1)
{
	return byte0 | (byte1 << 0x08);
}

uint32 MakeDWord(uint16 word0, uint16 word1)
{
	// Widen before shifting to avoid signed-int promotions for high bit 15.
	return rathena::byteorder::combine_words(word0, word1);
}

/*************************************
* Big-endian compatibility functions *
*************************************/

// Converts an int16 from current machine order to little-endian
int16 MakeShortLE(int16 val)
{
	return rathena::byteorder::native_to_little_endian(val);
}

// Converts an int32 from current machine order to little-endian
int32 MakeLongLE(int32 val)
{
	return rathena::byteorder::native_to_little_endian(val);
}

// Reads a uint16 in little-endian from the buffer
uint16 GetUShort(const unsigned char* buf)
{
	return rathena::byteorder::load_u16_le(std::span<const unsigned char, 2>(buf, 2));
}

// Reads a uint32 in little-endian from the buffer
uint32 GetULong(const unsigned char* buf)
{
	return rathena::byteorder::load_u32_le(std::span<const unsigned char, 4>(buf, 4));
}

// Reads a signed int32 from little-endian bytes, preserving every bit.
int32 GetLong(const unsigned char* buf)
{
	return rathena::byteorder::load_i32_le(std::span<const unsigned char, 4>(buf, 4));
}

// Reads IEEE-754 float bits in little-endian without strict-aliasing UB.
float GetFloat(const unsigned char* buf)
{
	return rathena::byteorder::load_f32_le(std::span<const unsigned char, 4>(buf, 4));
}

/// calculates the value of A / B, in percent (rounded down)
uint32 get_percentage(const uint32 A, const uint32 B)
{
	double result;

	if( B == 0 )
	{
		ShowError("get_percentage: divison by zero! (A=%u,B=%u)\n", A, B);
		return ~0U;
	}

	result = 100 * ((double)A / (double)B);

	if( result > UINT_MAX )
	{
		ShowError("get_percentage: result percentage too high! (A=%u,B=%u,result=%g)\n", A, B, result);
		return UINT_MAX;
	}

	return (uint32)floor(result);
}

uint32 get_percentage_exp(const uint64 a, const uint64 b)
{
	double result;

	if (b == 0)
	{
		ShowError("get_percentage_exp: divison by zero! (a=%" PRIu64 ",b=%" PRIu64 ")\n", a, b);
		return ~0U;
	}

	result = 100.0 * ((double)a / (double)b);

	if (result > UINT32_MAX)
	{
		ShowError("get_percentage_exp: result percentage too high! (a=%" PRIu64 ",b=%" PRIu64 ",result=%g)\n", a, b, result);
		return UINT32_MAX;
	}

	return (uint32)floor(result);
}
