
#ifndef PATTERN_H
#define PATTERN_H

#include "cstream.h"

typedef struct patterns_struct * Patterns;

typedef struct domain_struct * Domain;

Domain get_domain( const char* name );

int read_patterns ( CIStream s, const char* default_domain, boolean undef );

/* Read from `in', translate according to `p', and write the result
   to `out', until the next characters to be read matches `goal'.
   Returns TRUE on success, or FALSE if the translation fails or if
   end-of-file is reached before the goal string is found.  */
boolean translate ( CIStream in, Domain d, COStream out,
		    const unsigned char* goal );

extern boolean line_mode;

extern boolean discard_unmatched;

extern boolean case_insensitive;

extern char* idchars;
extern char* filechars;

extern boolean debug_switch;

extern boolean keep_going;

extern boolean ignore_whitespace;

extern COStream output_stream;

void pattern_help( FILE* f ); /* write help info to f */

void initialize_syntax(void);

boolean set_syntax( int type, const char* char_set );

#if 0
#define Arg_Delim '\0'
#endif

#endif
