
/* generalized macro processor */

/* $Id$ */

#if defined(_QC)
#pragma check_stack(off)
#endif

#include <stdio.h>
#include <ctype.h>	/* for isspace */
#include <string.h>
#include <assert.h>
#include "cstream.h"
#include "pattern.h"
#include "util.h"
#include "main.h"

COStream output_stream;
COStream stdout_stream;
CIStream stdin_stream;

boolean keep_going = FALSE;
boolean binary = FALSE;
Exit_States exit_status = EXS_OK;

void usage(void) {
  fprintf(stderr,
	  "Syntax: gema [ -p <patterns> ] [ -f <pattern-file> ] [ <in-file> <out-file> ]\n");
  fprintf(stderr, "Copies standard input to standard output, performing"
	  "\ntext substitutions specified by a series of patterns\n"
	  "specified in -p options or in files specified by "
	  "-f options.\n");
  pattern_help( stderr );
}

static struct switches {
  const char* name;
  boolean * var;
} switch_table[] =
  { { "line", &line_mode },
    { "b", &binary },
    { "k", &keep_going },
    { "match", &discard_unmatched },
    { "i", &case_insensitive },
    { "w", &ignore_whitespace },
    { "t", &token_mode },
    { "arglen", &MAX_ARG_LEN },
#ifndef NDEBUG
    { "debug", &debug_switch },
#endif
    { NULL, NULL } };

boolean
set_switch(const char* arg, boolean value) {
  struct switches *p;
  for ( p = &switch_table[0] ; p->name != NULL ; p++ )
    if ( stricmp(arg, p->name)==0 ) {
      *(p->var) = value;
      return TRUE;
    }
  return FALSE;
}

static struct parms {
  const char* name;
  char ** var;
} parm_table[] =
  { { "idchars", &idchars },
    { "filechars", &filechars },
    { "backup", &backup_suffix },
    { NULL, NULL } };

boolean
set_parm(const char* name, const char* value) {
  struct parms *p;
  for ( p = &parm_table[0] ; p->name != NULL ; p++ )
    if ( stricmp(name, p->name)==0 ) {
      char** vp = p->var;
#if 0 /* don't bother with this to avoid copying the initial value. */
      if ( *vp != NULL )
	free(*vp);
#endif
      *vp = str_dup(value);
      return TRUE;
    }
  return FALSE;
}

static char argv_domain_name[] = "ARGV";

static void
do_args(char** argv) {
  /* process the arguments according to the ARGV pattern domain */
  char** ap;
  CIStream argsbuf;
  COStream outbuf;
  const char* remaining;
  boolean ok;

  outbuf = make_buffer_output_stream();
  cos_putch(outbuf, '\n');
  for ( ap = argv ; *ap != NULL ; ap++ ) {
    cos_puts(outbuf, *ap);
    cos_putch(outbuf, '\n');
  }
  argsbuf = convert_output_to_input( outbuf );
  outbuf = make_buffer_output_stream();
  ok = translate ( argsbuf, get_domain(argv_domain_name), outbuf, NULL );
  cis_close(argsbuf);
  argsbuf = convert_output_to_input( outbuf );
  remaining = cis_whole_string(argsbuf);
  while ( isspace(*remaining) )
    remaining++;
  if ( remaining[0] != '\0' ) {
    fprintf(stderr, "Unrecognized arguments:\n%s", remaining);
    ok = FALSE;
  }
  cis_close(argsbuf);
  if ( !ok )
    exit_status = EXS_ARG;
}

#ifdef MSDOS
/* for MS-DOS conventions, use case-insensitive comparison for options */
#define CI "\\C"
#else
#define CI
#endif

/* The following rules define how the command-line arguments will
   be processed. */
static char argv_rules[] =
CI "\\N-h*\\n=@show-help@end\n"
   "\\A\\n\\Z=@err{@version\\N@show-help}@end\n"
CI "\\N-version\\n=@err{@version\\N}\n"
CI "\\N-f\\n*\\n=@set-switch{b;0}@define{@read{*}}@set-switch{b;${.BINARY;0}}\n"
CI "\\N-p\\n*\\n=@define{*}\n"
   "\\N-<L1>\\n=@set-switch{$1;1}\n"
CI "\\N-w\\n=@set-switch{w;1}@set-syntax{S;\\s\\t}\n"
CI "\\N-t\\n=@set-switch{w;1}@set-switch{t;1}\n"
CI "\\N-b\\n=@set-switch{b;1}@set{.BINARY;1}\n"
CI "\\N-arglen\\n<D>\\n=@set-switch{arglen;$1}\n"
CI "\\N-idchars\\n*\\n=@set-parm{idchars;$1}\n"
CI "\\N-filechars\\n*\\n=@set-parm{filechars;$1}\n"
CI "\\N-literal\\n*\\n=@set-syntax{L;$1}\n"
#ifndef NDEBUG
CI "\\N-debug\\n=@set-switch{debug;1}\n"
#endif
CI "\\N-line\\n=@set-switch{line;1}\n"
CI "\\N-match\\n=@set-switch{match;1}\n"
#ifdef MSDOS
   "\\N\\/<L1>\\n=@ARGV{-$1\\n}\n"	/* allow "/" instead of "-" */
#else
CI "\\N-n\\n=@set-switch{match;1}\n"	/* like for sed */
CI "\\N-e\\n*\\n=@define{*}\n"		/* like for sed */
#endif
CI "\\N-nobackup\\n=@set-parm{backup;}\n"
CI "\\N-backup\\n<G>\\n=@set-parm{backup;$1}\n"
CI "\\N-out\\n*\\n=@set{.OUT;$1}\n"
CI "\\N-in\\n*\\n=@set{.IN;$1}\n"
   "\\N\\L*\\=*\\n=@define{$0}\n"
   "\\N\\L\\@*\\{*\\n=@define{$0}\n"
CI "\\N-odir\\n*\\n=@set{.ODIR;*}\n"
CI "\\N-otyp\\n*\\n=@set{.OTYP;*}\n"
   "\\N-*\\n=@err{Unrecognized option:\\ \"-*\"\\n}@exit-status{3}\n"
   "\\n=\n"
   "\\N*\\n=@ARGV-FILE{${.ODIR;}\\n${.OUT;}\\n${.IN;}\\n*}\n"
   "\\Z=@ARGV-END{${.OUT;}\\n${.IN;}\\n${.ODIR;}\\n}\n"
"ARGV-FILE:\\n\\n\\n<U>=@set{.IN;$1};"
 "\\n\\n<U>\\n<U>=@set{.OUT;$2};"
 "\\n<U>\\n<U>\\n<U>=@err{More than two files specified.\\n}@exit-status{3};"
 "\\n<U>\\n*\\n<U>=@write{$1;@{@read{$3}}};"
 "<U>\\n\\n*\\n<U>=@bind{.OUT;@makepath{$1;@relpath{$3;$3};${.OTYP;}}}"
  "@write{${.OUT};@{@read{$3}}}@close{${.OUT}}@unbind{.OUT};"
 "<U>\\n<U>\\n=@err{Not meaningful\\:\\ both\\ -out\\ and\\ -odir\\n}"
	"@exit-status{3}@end\n"
"ARGV-END:\\n\\n<U>\\n=@end;" /* -odir was specified */
 "<U>\\n\\n\\n=@end;"
 "<U>\\n<U>\\n=@write{$1;@{@read{$2}}}@end;" /* output and input files */
 "\\n\\n\\n=@write{-;@{@read{-}}};" /* no files specified, use stdin/stdout */
 "\\n<U>\\n=@write{-;@{@read{$1}}}@end\n"
#ifndef NDEBUG
  "ARGV-FILE:*=@err{Internal failure in ARGV-FILE \"*\"\\n}\n"
  "ARGV-END:*=@err{Internal failure in ARGV-END \"*\"\\n}\n"
#endif
;

static void
initialize_argv_domain(void) {
  CIStream str;
  assert( (int)EXS_ARG == 3 ); /* to match "@exit-status" above */
  str = make_string_input_stream(argv_rules, sizeof(argv_rules)-1, FALSE);
  read_patterns ( str, argv_domain_name, FALSE );
  cis_close(str);
}

int main(int argc, char* argv[]){
  char** ap;
  initialize_syntax();
  ap = argv+1;
  stdout_stream = make_file_output_stream(stdout,"");
  stdin_stream = make_file_input_stream(stdin,NULL);
  output_stream = stdout_stream;
  if ( argc >= 3 && stricmp(*ap,"-prim") == 0 ) {
    CIStream ps;
    ap++;
    ps = open_input_file( *ap, FALSE );
    if ( ps != NULL ) {
      read_patterns(ps, "", FALSE);
      cis_close(ps);
    }
    ap++;
  }
  else initialize_argv_domain();
  do_args(ap);
  return (int)exit_status;
}

