#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

typedef enum {
  MemoryPatterns, MemoryStream, MemoryInputBuf, MemoryOutputBuf,
  MemoryVar, MemoryPath, MemoryRegexp, MemoryDispatch
} Memory_Kinds;

/* allocate memory space; does not return unless succesful */
void* allocate( size_t size, Memory_Kinds what );

char* str_dup( const char* x ); /* return new copy of string */
char* str_dup_len( const char* x, int len );

#ifdef MSDOS
#define DirDelim '\\'
#else   /* assume Unix */
#define DirDelim '/'
#endif

const char*
pathname_name_and_type(const char* path);

const char*
pathname_type(const char* path);

char*
pathname_merge_directory( const char* path, const char* dir );

int
is_absolute_pathname(const char* path);

const char*
relative_pathname(const char* relative_to, const char* file_path);

#endif

