/* character stream functions */

/* $Id$ */

#if defined(_QC)
#pragma check_stack(off)
#endif

#include "cstream.h"
#include "util.h"
#include "main.h"
#include <string.h>
#include <assert.h>
#include <stdarg.h>

/* for the `stat' struct: */
#include <sys/types.h>
#include <sys/stat.h>

/* ============    Input Streams ==============  */

struct input_stream_struct
{
  FILE* fs;
  MarkBuf* first_mark;
  unsigned char* start;
  unsigned char* end;
  const unsigned char* next;
  unsigned char* bufend;
  int peek_char;
  int cur_char;
  const char* pathname;
  unsigned long line;
  unsigned short column;
};

#define is_string_stream(s) (s->next!=NULL)
#define is_file_stream(s) (s->fs!=NULL)

#define NoMark NULL
#define BufSize 4000

static boolean
extend_buffer ( CIStream s ) {
	if ( s->first_mark == NoMark ) { /* don't need buffer anymore */
	  s->cur_char = cis_prevch(s);
	  free( s->start );
	  s->start = NULL; s->next = NULL; s->bufend = NULL;
	  s->peek_char = -1;
	}
	else if ( s->end < (s->bufend-2) ) { /* read another line in buffer */
	  unsigned char* r;
	  r = (unsigned char*)
	    fgets((char*)s->end, (s->bufend - s->end), s->fs);
	  if ( r != NULL )
	    s->end = r + strlen((char*)r);
	  else return FALSE;
	}
       else { /* need to expand the buffer */
	 unsigned char* x;
	 size_t n;
	 size_t newsize;
	 n = s->end - s->start;
	 newsize = (s->bufend - s->start) + BufSize;
	 x = realloc( s->start, newsize );
	 if ( x != NULL ) { /* expanded OK */
	   s->start = x;
	   s->end = x + n;
	   s->next = s->end;
	   s->bufend = x + newsize;
	 }
	 else {
	   fputs("Out of memory for expanding input buffer.\n",stderr);
	   exit(EXS_MEM);
	 }
       }
   return TRUE;
}

/* read one character or EOF */
int cis_getch(CIStream s){
  int ch;
  if ( is_string_stream(s) ) {
    if ( s->next >= s->end ) {
      if ( is_file_stream(s) ) { /* buffered file stream */
	if ( extend_buffer(s) )
	  return cis_getch(s);
      }
      return EOF;
    }
    if ( s->next[-1] == '\n' ) {
      s->line++;
      s->column = 1;
    }
    else s->column++;
    return *s->next++;
  }
  else {
    if ( s->cur_char == '\n' ) {
      s->line++;
      s->column = 1;
    }
    else s->column++;
    if ( s->peek_char >= 0 ) {
      ch = s->peek_char;
      s->peek_char = -1;
    } else {
      ch = getc(s->fs);
      if ( ch == EOF && !feof(s->fs) ) {
	perror(s->pathname? s->pathname : "stdin");
	if ( keep_going )
	  exit_status = EXS_INPUT;
	else exit(EXS_INPUT);
	}
    }
    s->cur_char = ch;
    return ch;
  }
}

/* peek ahead to the next character to be read */
int cis_peek(CIStream s){
  if ( is_string_stream(s) ) {
    if ( s->next >= s->end ) {
      if ( is_file_stream(s) ) { /* buffered file stream */
	if ( extend_buffer(s) )
	  return cis_peek(s);
      }
      return EOF;
    }
    else return *s->next;
  }
  else {
    if ( s->peek_char < 0 )
      s->peek_char = getc(s->fs);
    return s->peek_char;
  }
}

/* return the previous character read */
int cis_prevch(CIStream s){
  if ( is_string_stream(s) ) {
    if ( s->next <= s->start )
      return EOF;
    else return s->next[-1];
  }
  else return s->cur_char;
}

unsigned long cis_line(CIStream s) {
  return s==NULL? (unsigned long)0 : s->line;
}

unsigned int cis_column(CIStream s) {
  return s==NULL? 0 : s->column;
}

/* remember current position */
void cis_mark(CIStream s,MarkBuf* mb){
  long m;
  if ( is_string_stream(s) )
    m = s->next - s->start;
  else {
#ifndef MSDOS /* seek is too slow on MS-DOS */
    if ( s->pathname == NULL ) /* can't trust rewinding a pipe */
      m = -1;
    else m = ftell(s->fs);
    if ( m < 0 ) /* seek is not supported; need to buffer */
#endif
     {
      unsigned char* r;
      r = (unsigned char*) allocate( BufSize, MemoryInputBuf );
      s->start = r;
      if ( s->cur_char != EOF )
	*r++ = (unsigned char)s->cur_char;
      s->next = r;
      if ( s->peek_char >= 0 )
	*r++ = (unsigned char)s->peek_char;
      s->end = r;
      s->bufend = s->start + BufSize;
      m = s->next - s->start;
    }
  }
  if ( s->first_mark == NoMark )
    s->first_mark = mb;
  else assert( m >= s->first_mark->position );
  mb->position = m;
  mb->line = s->line;
  mb->column = s->column;
  mb->prevch = s->cur_char;
  mb->peekch = s->peek_char;
}

void cis_release(CIStream s, MarkBuf* mb) {
  if ( mb == s->first_mark ) {
      s->first_mark = NoMark;
      if ( is_string_stream(s) && is_file_stream(s) &&
	   (s->end - s->next)*2 < (s->next - s->start) ){
	unsigned char* n;
	size_t len = s->end - s->next;
	memmove( s->start, s->next-1, len+1 );
	n = s->start+1;
	s->next = n;
	s->end = n + len;
	*s->end = '\0';
      }
  }
  else assert( s->first_mark != NoMark );
}

/* restore to remembered position; returns 0 if OK */
int cis_restore(CIStream s, MarkBuf* mb){
  int status;
  long pos;
  pos = mb->position;
  if ( is_string_stream(s) ) {
    s->next = s->start + pos;
    if ( s->next < s->start || s->next > s->end ) {
      fprintf(stderr, "cis_restore invalid position %ld\n", pos);
      exit(EXS_INPUT);
    }
    status = 0;
  }
  else {
    status = fseek(s->fs, pos, SEEK_SET);
  }
  cis_release(s, mb);
  s->line = mb->line;
  s->column = mb->column;
  s->cur_char = mb->prevch;
  s->peek_char = mb->peekch;
  return status;
}

/* reset to read from the beginning */
void cis_rewind(CIStream s){
  if ( is_string_stream(s) )
    s->next = s->start;
  else {
    s->peek_char = -1;
    s->cur_char = EOF;
    fseek(s->fs, 0, SEEK_SET);
  }
}

/* create stream */
CIStream make_file_input_stream(FILE* f, const char* path){
  CIStream s = (CIStream) allocate( sizeof(struct input_stream_struct),
				MemoryStream );
  s->fs = f;
  s->pathname = str_dup(path);
  s->first_mark = NoMark;
  s->peek_char = -1;
  s->cur_char = EOF;
  s->line = 1;
  s->column = 1;
  s->next = NULL;
  s->start = NULL;
  s->end = NULL;
  s->bufend = NULL;
  return s;
}

CIStream
open_input_file( const char* pathname, boolean binary )
{
  FILE* infs;
  if ( pathname[0] == '-' && pathname[1] == '\0' )
    return stdin_stream;
  infs = fopen( pathname, binary? "rb" : "r" );
  if ( infs == NULL ) {
    input_error(input_stream, EXS_INPUT, "Can't open file for reading:\n");
    perror(pathname);
    if ( keep_going )
      return NULL;
    else exit(EXS_INPUT);
  }
  return make_file_input_stream(infs,pathname);
}

const char* cis_pathname(CIStream s){
  return s==NULL? "" : s->pathname;
}

boolean cis_is_file(CIStream s) {
  return is_file_stream(s);
}

time_t cis_mod_time(CIStream s) {
  if ( s != NULL && s->fs != NULL && s->pathname != NULL ) {
    struct stat sbuf;
    fstat( fileno(s->fs), &sbuf );
    return sbuf.st_mtime;
  }
  else return 0;
}

char probe_pathname(const char* pathname) {
    struct stat sbuf;
    if ( stat( pathname, &sbuf ) == 0 ) {
      if ( sbuf.st_mode & S_IFDIR )
	return 'D'; /* directory */
      else if ( sbuf.st_mode & S_IFREG )
	return 'F'; /* file */
      else if ( sbuf.st_mode & S_IFCHR )
	return 'V'; /* device */
      else return 'X'; /* unexpected mode */
    }
    else return 'U'; /* undefined */
}

CIStream make_string_input_stream(const char* x, size_t length,
				boolean copy){
  CIStream s = (CIStream) allocate( sizeof(struct input_stream_struct),
			MemoryStream );
  if ( length == 0 )
    length = strlen(x);
  s->fs = NULL;
  s->pathname = NULL;
  s->first_mark = NoMark;
  s->start = (unsigned char*)x;
  s->bufend = NULL;
  if ( copy ) {
    size_t alen = length+1;
    s->start = allocate(alen, MemoryInputBuf);
    memcpy(s->start, x, alen);
    s->bufend = s->start + alen;
  }
  s->next = s->start;
  s->end = s->start + length;
  s->line = 0;
  s->column = 0;
  return s;
}

CIStream clone_input_stream( CIStream in ) {
  CIStream s = (CIStream) allocate( sizeof(struct input_stream_struct),
			MemoryStream );
  assert ( is_string_stream(in) );
  *s = *in;
  s->first_mark = NoMark;
  s->bufend = NULL;
  return s;
}

char* cis_whole_string ( CIStream s ) {
  if ( s == NULL )
    return "";
  else {
    assert( is_string_stream(s) );
    assert( *s->end == '\0' );
    return (char*)s->start;
  }
}

/* number of characters in input buffer stream */
unsigned cis_length( CIStream s ){
  if ( s == NULL )
    return 0;
  else {
    assert( is_string_stream(s) );
    return s->end - s->start;
  }
}

/* close stream */
void cis_close(CIStream s){
  if ( s == NULL || s == stdin_stream )
    return;
  if ( is_file_stream(s) ) {
    fclose(s->fs);
    s->fs = NULL;
    if ( s->pathname != NULL ) {
      free((char*)s->pathname);
      s->pathname = NULL;
    }
  }
  s->next = NULL;
  if ( s->bufend != NULL ) {
    free( s->start );
    s->start = NULL;
    s->bufend = NULL;
  }
  free(s);
}

void input_error( CIStream s, Exit_States code, const char* format, ... ) {
  va_list args;
  va_start(args,format);
  if ( code > exit_status )
    exit_status = (Exit_States)code;
  if ( s != NULL ) {
    if ( !is_file_stream(s) )
      s = input_stream;
    if ( s != NULL && is_file_stream(s) ) {
      const char* path = cis_pathname(s);
      if ( path != NULL )
	fprintf(stderr, "File \"%s\" line %ld: ",
		pathname_name_and_type(path),
		cis_line(s) );
    }
  }
  vfprintf(stderr, format, args);
  va_end(args);
}


/* ============    Output Streams ==============  */

struct output_stream_struct {
  FILE* fs;   /* the file descriptor */
  int lastch; /* last character written */
  int column;
  unsigned char* start;
  unsigned char* next;
  unsigned char* bufend;
  const char* pathname;
};

#define OutBufSize 512

static void
expand_output_buffer ( COStream s, int need ) {
   unsigned char* x;
   size_t n;
   size_t more;
   size_t oldsize;
   size_t newsize;

   oldsize = s->bufend - s->start;
   more = OutBufSize + (oldsize >> 2) ;
   if ( (size_t)need >= more )
     more = need + OutBufSize;
   newsize = oldsize + more;
   n = s->next - s->start;
   x = realloc( s->start, newsize );
   if ( x != NULL ) { /* expanded OK */
     s->start = x;
     s->next = x + n;
     s->bufend = x + newsize;
   }
   else {
     fputs("Out of memory for expanding output buffer.\n",stderr);
     exit(EXS_MEM);
   }
}

/* write one character */
void cos_putch(COStream s, int c){
  if ( is_string_stream(s) ) {
    if ( s->next >= s->bufend )
      expand_output_buffer ( s, 1 );
    *s->next++ = c;
  }
  else {
    if ( putc(c,s->fs) == EOF ) {
      fprintf(stderr, "Error writing to output file:\n");
      perror(s->pathname);
      exit(EXS_OUTPUT);
    }
    s->lastch = c;
    if ( c == '\n' )
      s->column = 1;
    else s->column++;
  }
}

/* write some number of spaces */
void cos_spaces(COStream s, int n) {
  int i;
  for ( i = n ; i > 0 ; i-- )
    cos_putch(s,' ');
}

/* write null-terminated string */
void cos_puts(COStream s, const char* x) {
  int len;
  if ( x == NULL )
    return;
  len = strlen(x);
  if ( len > 0 ) {
    if ( is_string_stream(s) )
      cos_put_len(s, x, len);
    else {
      int n;
      const char* p;
      fputs(x,s->fs);
      p = x+len-1;
      s->lastch = *p;
      for ( n = 0 ; p >= x ; p-- ) {
	if ( *p == '\n' ) {
	  s->column = 1;
	  break;
	}
	else n++;
      }
      s->column += n;
    }
  }
}

/* write arbitrary data */
void cos_put_len(COStream s, const char* x, size_t len) {
  if ( len > 0 ) {
    if ( is_string_stream(s) ) {
      if ( s->next + len >= s->bufend )
	expand_output_buffer ( s, len );
      memcpy( s->next, x, len+1 );
      s->next += len;
    }
    else {
      int n;
      const char* p;
      p = x;
      for ( n = len ; n > 0 ; n-- )
	cos_putch(s,*p++);
    }
  }
}

/* return the last character written, or EOF if nothing written yet */
int cos_prevch(COStream s) {
  if ( is_string_stream(s) )
    return s->next <= s->start ? EOF : s->next[-1];
  else return s->lastch;
}

unsigned int cos_column(COStream s) {
  if ( s == NULL )
    return 0;
  else if ( is_string_stream(s) ) {
    int n;
    const unsigned char* p;
    n = 1;
    for ( p = s->next-1 ; p >= s->start ; p-- ) {
      if ( *p == '\n' )
	break;
      else n++;
    }
    return n;
  }
  else return s->column;
}

/* start new line if not already at start of line */
void cos_freshline( COStream s ) {
  int lastch;
  lastch = cos_prevch(s);
  if ( lastch != '\n' && lastch != EOF )
    cos_putch(s,'\n');
}

#if 0  /* don't think this is needed */
/* remember current position */
CSMark cos_mark(COStream s){
  return (CSMark) ftell(s->fs);
}

/* restore to remembered position; returns 0 if OK */
int cos_restore(COStream s, CSMark pos){
  return fseek(s->fs, pos, SEEK_SET);
}
#endif

/* create stream */
COStream make_file_output_stream(FILE* f, const char* path){
  COStream s = (COStream) allocate( sizeof(struct output_stream_struct),
		MemoryStream );
  s->fs = f;
  s->pathname = str_dup(path);
  s->lastch = EOF;
  s->column = 1;
  s->start = NULL;
  s->next = NULL;
  s->bufend = NULL;
  return s;
}

char* backup_suffix = ".bak";

char* current_backup = NULL;
static COStream current_output = NULL;

COStream
open_output_file( const char* pathname, boolean binary )
{
  FILE* outfs;
  if ( pathname[0] == '-' && pathname[1] == '\0' )
    return stdout_stream;
  if ( current_backup != NULL ) {
    free(current_backup);
    current_backup = NULL;
  }
  if ( backup_suffix[0] != '\0' ) {
    CIStream bakpath;
    COStream outbuf;
    const char* backup_pathname;
    outbuf = make_buffer_output_stream();
#ifdef unix
    /* on Unix, append ".bak" to the file name */
    cos_puts(outbuf,pathname);
    cos_puts(outbuf,backup_suffix);
#else
    /* on MS-DOS, replace any previous extension */
    merge_pathnames( outbuf, FALSE, NULL, pathname, backup_suffix);
#endif
    bakpath = convert_output_to_input( outbuf );
    backup_pathname = cis_whole_string(bakpath);
    remove(backup_pathname);
    if ( rename(pathname,backup_pathname) == 0 ) {
      current_backup = str_dup(backup_pathname);
    }
    cis_close(bakpath);
  }

  outfs = fopen( pathname, binary? "wb" : "w" );
  if ( outfs == NULL ) {
    input_error(input_stream, EXS_OUTPUT, "Can't open output file:\n");
    perror(pathname);
    if ( keep_going )
      return NULL;
    else exit(EXS_OUTPUT);
  }
  else {
    current_output = make_file_output_stream(outfs,pathname);
    return current_output;
  }
}

COStream make_buffer_output_stream(){
  COStream s = (COStream) allocate( sizeof(struct output_stream_struct),
		MemoryOutputBuf );
  s->fs = NULL;
  s->pathname = NULL;
  s->lastch = EOF;
  s->column = 0;
  s->start = (unsigned char*) allocate( OutBufSize, MemoryOutputBuf );
  s->next = s->start;
  s->bufend = s->start + OutBufSize;
  return s;
}

const char* cos_pathname(COStream s){
  return s==NULL? "" : s->pathname;
}

/* number of characters written so far to output buffer stream */
unsigned cis_out_length( COStream s ){
  if ( is_file_stream(s) )
    return (unsigned)ftell(s->fs);
  else return s->next - s->start;
}

/* close stream */
void cos_close(COStream s){
  if ( s == NULL || s == stdout_stream )
    return;
  if ( is_file_stream(s) ) {
    fclose(s->fs);
    s->fs = NULL;
  }
  if ( is_string_stream(s) ) {
    free(s->start);
    s->start = NULL;
    s->next = NULL;
  }
  if ( s->pathname != NULL ) {
      free((char*)s->pathname);
      s->pathname = NULL;
    }
  if ( s == current_output )
    current_output = NULL;
  free(s);
}

CIStream convert_output_to_input( COStream out ) {
  size_t len;
  unsigned char* buf;
  CIStream in;

  assert( is_string_stream(out) && !is_file_stream(out) );
  len = out->next - out->start;
  buf = realloc( out->start, len+1 ); /* release unused space */
  buf[len] = '\0';
  in = make_string_input_stream((char*)buf, len, FALSE);
  in->bufend = buf + len+1; /* cause free when input stream is closed */
  out->start = NULL;
  out->next = NULL;
  free(out);
  return in;
}

void cos_copy_input_stream(COStream out, CIStream in) {
  if ( in != NULL )
    if ( is_file_stream(in) ) {
      int ch;
      while ( (ch = cis_getch(in)) != EOF )
	cos_putch(out,ch);
    }
    else {
      cos_put_len(out, (const char*)in->next, in->end - in->next);
      in->next = in->end;
    }
}
