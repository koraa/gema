
/* pattern matching */

/* $Id$ */

/*********************************************************************
  This file is part of "gema", the general-purpose macro translator,
  written by David N. Gray <dgray@acm.org> in 1994 and 1995.
  You may do whatever you like with this, so long as you retain
  an acknowledgment of the original source.
 *********************************************************************/

#include "pattern.h"
#include "util.h"
#include "patimp.h"
#include "var.h"	/* for get_var */
#include <ctype.h>  /* for isalnum, isspace */
#include <string.h>
#include <assert.h>

boolean case_insensitive = FALSE;
boolean ignore_whitespace = FALSE;

int MAX_ARG_LEN = 4096;

int arg_char;

CIStream input_stream = NULL; /* used only for error message location */

enum Translation_Status translation_status;

char* idchars = "_";
char* filechars = "./-_~#@%+="
#ifdef MSDOS
		":\\"
#endif
		;

boolean
isident( int ch ) { /* is the character an identifier constituent? */
  return isalnum(ch) ||
    ( strchr(idchars,ch) != NULL && ch != 0 );
}

#if defined(_QC) || defined(_MSC_VER) /* Microsoft C or Quick C */
#pragma check_stack(on)
#endif

static CIStream
match_regexp ( CIStream s, int regex_num ) {
      int lc;
      unsigned char* end_match;
      MarkBuf begin;
      unsigned char buf[1024];
      unsigned char* tp;
      int tc;

      lc = cis_prevch(s);
      cis_mark(s, &begin);
      for ( tp = buf ; tp < buf+1023 ; ) {
	tc = cis_getch(s);
	if ( tc == EOF )
	  break;
	*tp++ = (unsigned char)tc;
      	if ( tc == '\n' )
	  break;
      }
      *tp = '\0';
      end_match = regex_match( regex_num, buf, (lc == '\n' || lc == EOF ));
      cis_restore(s, &begin);
      if ( end_match == NULL )
	return NULL;
      else {
	*end_match = '\0';
	for ( tp = buf ; tp < end_match ; tp++ )
	  (void)cis_getch(s);
	return make_string_input_stream((char*)buf, end_match-buf, TRUE);
      }
}

static unsigned
get_goal_char( const unsigned char* ps ) {
  /* returns goal character, or ENDOP if no goal, or a value that
     satisfies ISOP for an operator. */
  return get_template_element(&ps,TRUE);
}

/* option bits for try_pattern */
#define MatchSwallow 1
#define MatchLine 2
#define MatchNoCase 4

struct mark_struct {
  CIStream in;
  boolean marked;
  MarkBuf start;
};

static int
getch_marked(struct mark_struct* ps) {
  if ( !ps->marked ) {
    cis_mark(ps->in, &ps->start);
    ps->marked = TRUE;
  }
  return cis_getch(ps->in);
}

boolean
try_pattern( CIStream in, const unsigned char* patstring, CIStream* next_arg,
	     CIStream* all_args, int options, const unsigned char* goal) {
  const unsigned char* ps;
  int ic, pc;
  struct mark_struct marker;
  MarkBuf end_position;
  boolean end_position_marked;
  boolean match;
  int local_options;

  local_options = options;
  marker.marked = FALSE;
  marker.in = in;
  end_position_marked = FALSE;
  for ( ps = patstring ; ; ps++ ) {
    pc = *ps;
    switch(pc){
    case PT_END:
      goto success;
    case PT_MATCH_ANY: {
      COStream outbuf;
      int limit;
      const unsigned char* use_goal;
      if ( next_arg == NULL ) /* matching only up to first argument */
	goto success;
      assert( next_arg[0] == NULL );
      next_arg[1] = NULL;
      outbuf = make_buffer_output_stream();
      use_goal = NULL;
      if ( ps[1] == PT_END )
	use_goal = goal;
      for ( limit = MAX_ARG_LEN ; limit > 0 ; limit-- ) {
      	ic = cis_peek(in);
	if ( ic == EOF ) {
	  if ( ps[1] == PT_END || ps[1] == PT_AUX ) {
	    *next_arg++ = convert_output_to_input(outbuf);
	    goto continue_match;
	  }
	  else break;
	}
	if ( use_goal != NULL ) {
	  if ( try_pattern( in, use_goal, NULL, NULL,
			    options&MatchNoCase, goal ) &&
		use_goal[0] != PT_END ) {
	    next_arg[0] = convert_output_to_input(outbuf);
	    goto success;
	  }
	}
	else
	if ( try_pattern( in, ps+1, next_arg+1, all_args,
			  (local_options | MatchSwallow), goal ) &&
	     ps[1] != PT_END ) {
	  next_arg[0] = convert_output_to_input(outbuf);
	  goto success;
	}
	if ( ic == '\n' && (local_options & MatchLine) )
	  goto failed_any;
	else {
	  int xc;
	  xc = getch_marked(&marker);
#ifndef NDEBUG
	  if ( xc != ic )
	    input_error(in, EXS_INPUT, __FILE__
	        " line %d: ic = '%c', xc = '%c'\n",
	         __LINE__, ic, xc );
#endif
	  cos_putch(outbuf, (char)xc );
	}
      }
    failed_any:
      cos_close(outbuf);
      goto failure;
    } /* end PT_MATCH_ANY */

    case PT_MATCH_ONE: {  /* "?" argument (general case) */
      char str[2];
      if ( next_arg == NULL ) /* matching only up to first argument */
	goto success;
      ic = getch_marked(&marker);
      if ( ic == EOF || ( ic == '\n' && (local_options & MatchLine) ) )
	goto failure;
      str[0] = ic;
      str[1] = '\0';
      *next_arg++ = make_string_input_stream(str, 1, TRUE);
      *next_arg = NULL;
      break;
    } /* end PT_MATCH_ONE */

    case PT_ONE_OPT: {  /* "?" argument (optimized special case) */
      assert( next_arg != NULL );
      arg_char = cis_getch(in);
      if ( arg_char == EOF )
	goto failure;
      break;
    } /* end PT_ONE_OPT */

    case PT_RECUR: { /* argument recursively translated */
      COStream outbuf;
      int domain;
      if ( next_arg == NULL ) /* matching only up to first argument */
	goto success;
      assert( next_arg[0] == NULL );
      outbuf = make_buffer_output_stream();
      domain = *++ps - 1;
      if ( !marker.marked ) {
	cis_mark(in,&marker.start);
	marker.marked = TRUE;
      }
      if ( translate ( in, domains[domain], outbuf,
		       ( ps[1]==PT_END? goal : ps+1 ) ) ) {
	*next_arg++ = convert_output_to_input( outbuf );
	*next_arg = NULL;
	}
      else {
	cos_close(outbuf);
	goto failure;
      }
      break;
    } /* end PT_RECUR */

    case PT_SPECIAL_ARG: {
      COStream outbuf;
      int kind;
      int parms;
      int num_wanted;
      int num_found;
      boolean optional;
      boolean inverse;
      unsigned goal_char;
      num_found = 0;
      parms = *++ps;
      optional = parms & 0x40;
      inverse = parms & 0x80;
      kind = ('A'-1) + (parms & 0x3F);
      num_wanted = (*++ps) - 1;
      goal_char = get_goal_char(ps+1);
      if ( goal_char == ENDOP && goal != NULL )
	goal_char = get_goal_char(goal);
      if ( num_wanted >= 0xFE )
	num_wanted = -1;
      else if ( !optional )
	goal_char = ENDOP;
      outbuf = next_arg != NULL ? make_buffer_output_stream() : NULL;
      for ( ; ; ) {
	boolean ok;
	ic = cis_peek(in);
	switch(kind) {
	case 'O': if ( ic > '7' ) {			/* octal digits */
		    ok = FALSE;
		    break;
		}
	  /* else fall-through */
	case 'D': ok = isdigit(ic); break;		/* digits */
	case 'W': if ( num_found > 0 &&			/* word */
		       ( ic == '\'' || ic == '-' ) ) {
		    ok = TRUE;
		    break;
		}
	  /* else fall-through */
	case 'L': ok = isalpha(ic); break;		/* letters */
	case 'A': ok = isalnum(ic); break;		/* alphanumerics */
	case 'I': ok = isident(ic); break;		/* identifier */
	case 'G': ok = isgraph(ic); break;		/* graphic char */
	case 'C': ok = iscntrl(ic); break;		/* control char */
	case 'F': ok = isalnum(ic) ||			/* file pathname */
		( strchr(filechars,ic)!=NULL && ic!=0 );
		break;
	case 'S': ok = isspace(ic); break;		/* white space */
	case 'X': ok = isxdigit(ic); break;		/* hex digits */
	case 'N': ok = isdigit(ic) || 			/* number */
			( ic=='.' && cos_prevch(outbuf) != '.' ) ||
			( num_found == 0 && ( ic=='-' || ic=='+' ) );
		if ( !ok && num_found == 1 &&
		     strchr("+-.", cos_prevch(outbuf)) != NULL ) {
		  cos_close(outbuf);
		  goto failure;
		  }
		break;
	case 'Y': ok = ispunct(ic) &&			/* punctuation */
			 !( isspace(ic) | isident(ic) );
		break;
	case 'P': ok = isprint(ic); break;		/* printing char */
	case 'T':
	case 'V': ok = (isprint(ic) | isspace(ic));	/* valid text */
		break;
	case 'U': ok = ic != EOF;			/* anything */
		break;
	default:
		fprintf(stderr, "Undefined arg type: <%c>\n", kind);
		ok = FALSE;
	}
	if ( inverse && ic != EOF )
	  ok = !ok;
	if(ok) {
	  if ( ic == '\n' && (local_options & MatchLine) )
	    break;
	  if ( next_arg == NULL ) /* matching only up to first argument */
	    goto success;
	  if ( ( ic == goal_char ||
	         ( goal_char == UPOP(PT_SPACE) && isspace(ic) ) ) &&
	       try_pattern( in, ps+1, NULL, all_args,
			    (options & MatchNoCase), goal ) )
	    /* would be valid constituent except that it appears in the
	       template as a terminator. */
	    break;
	  num_found++;
	  if ( num_found > num_wanted && num_wanted >= 0 ) {
	    if ( num_wanted == 0 )
	      cos_putch(outbuf, ic);
	    break;
	  }
	  cos_putch(outbuf, getch_marked(&marker));
	} /* end ok */
	else if ( ignore_whitespace && isspace(ic) && num_found == 0 &&
		  ( kind=='Y' || kind=='C' || !isident(cis_prevch(in)) ) ) {
	  (void)getch_marked(&marker);
	  continue;
	}
	else break;
      } /* end for */
      if ( ( num_found < num_wanted ||  /* not enough characters found */
	     num_found == 0		/* no valid characters found */
	   ) && !optional )
       {
	cos_close(outbuf);
      	goto failure;
      }
      else {
	if ( next_arg != NULL ) {
	  *next_arg++ = convert_output_to_input(outbuf);
	  *next_arg = NULL;
	}
	break;
      }
    } /* end PT_SPECIAL_ARG */

    case PT_REGEXP: {
       CIStream value;
       value = match_regexp ( in, *++ps -1 );
       if ( value == NULL )
	 goto failure;
       *next_arg++ = value;
       *next_arg = NULL;
       break;
    }

    case PT_PUT_ARG: { /* match against value of previous argument */
	CIStream arg;
	int ac;
	if ( next_arg == NULL ) /* matching only up to first argument */
	  goto success;
	arg = all_args[ (*++ps) - 1 ];
	if ( arg == NULL )
	  goto failure;
	cis_rewind(arg);
	while ( ( ac = cis_getch(arg) ) != EOF ) {
	  if ( ac == cis_peek(in) )
	    (void)getch_marked(&marker);
	  else goto failure;
	}
	break;
    }

    case PT_VAR1: {
	char vname[2];
	const unsigned char* value;
	size_t length;
	vname[0] = *++ps;
	vname[1] = '\0';
    	value = (const unsigned char*) get_var(vname,FALSE,&length);
	for ( ; length > 0 ; length-- )
	  if ( ((int)*value++) == cis_peek(in) )
	    (void)getch_marked(&marker);
	  else goto failure;
	break;
      }

    case PT_SPACE: /* at least one space required */
      if ( !isspace( cis_peek(in) ) ) {
	if ( isspace( cis_prevch(in) ) &&
	     ( marker.marked || next_arg == all_args ) )
	  break;
	else goto failure;
      }
      /* and fall through for optional additional space */
    case PT_SKIP_WHITE_SPACE: {  /* optional white space */
      int x;
      for ( ; ; ) {
	x = cis_peek(in);
	if ( x == EOF || !isspace(x) )
	  break;
	if ( x == '\n' &&
	     ( (local_options & MatchLine) || ps[1] == PT_LINE ||
      	       ps[1] == '\n' ||
	       ( ps > patstring &&
	         ( ps[-1] == PT_LINE || ps[-1] == '\n' ) ) ) )
	  break;
	(void)getch_marked(&marker);
      }
      break;
    }
    case PT_WORD_DELIM: { /* word delimiter */
      if ( !isalnum( cis_prevch(in) ) )
	break;
      if ( !isalnum(cis_peek(in)) )
	break;
      goto failure;
    }
    case PT_ID_DELIM: { /* identifier delimiter */
      if ( !isident( cis_prevch(in) ) )
	break;
      if ( !isident(cis_peek(in)) )
	break;
      goto failure;
    }
#if 0 	/* changed my mind */
    case PT_ARG_DELIM: { /* command line argument delimiter */
      while ( cis_peek(in) == Arg_Delim )
	(void)getch_marked(&marker);
      ic = cis_prevch(in);
      if ( ic == Arg_Delim || ic == EOF || cis_peek(in) == EOF )
	break;
      else goto failure;
    }
#endif
   case PT_LINE: { /* at beginning or end of line */
      int np;
      ic = cis_prevch(in);
      if ( ic == '\n' || ic == EOF )
	break;
      np = ps[1];
      if ( np != PT_SKIP_WHITE_SPACE && np != PT_SPACE &&
   	   np != PT_LINE && !isspace(np) ) {
	ic = cis_peek(in);
	if ( ic == EOF )
	  break;
	if ( ic == '\n' ) {
	  if ( !is_operator(np) ) /* accept newline if not end of template */
	    (void)getch_marked(&marker);
	  break;
	}
      }
      goto failure;
    }

   case PT_AUX: {
     unsigned char ec;
     ec = *++ps;
     switch(ec) {
#if 0	/* superseded by separate beginning and end operators */
       case PTX_MATCH_EOF: { /* at beginning or end of file */
	if ( cis_is_file(in) ) {
	  if ( cis_prevch(in) == EOF )
	    break;
	  if ( cis_peek(in) == EOF )
	    break;
	}
	goto failure;
      }
#endif
      case PTX_ONE_LINE:
	local_options |= MatchLine;
	break;
      case PTX_NO_CASE:
	local_options |= MatchNoCase;
	break;
      case PTX_BEGIN_FILE:
	if ( !cis_is_file(in) )
	  goto failure;
	/* else fall-through */
      case PTX_INIT: /* beginning of input data */
	if ( cis_prevch(in) == EOF && ps[1] != PT_END )
	  break;
	else goto failure;
      case PTX_END_FILE:
	if ( !cis_is_file(in) )
	  goto failure;
	/* else fall-through */
      case PTX_FINAL: /* end of input data */
	if ( cis_peek(in) == EOF && ps > patstring )
	  break;
    	else goto failure;
      case PTX_POSITION: /* leave input stream here after match */
	if ( ps[1] != PT_END ) {
	  if ( !marker.marked ) {
	    cis_mark(in,&marker.start);
	    marker.marked = TRUE;
	  }
	  cis_mark(in, &end_position);
	  end_position_marked = TRUE;
	}
	break;
      case PTX_NO_GOAL:
	if ( !(local_options & MatchSwallow) ) {
	  /* when doing look-ahead for argument delimiter */
	  assert( next_arg == NULL );
	  if ( marker.marked ) /* some text matched, consider it sufficient. */
	    goto success;
	  else goto failure; /* don't terminate the argument yet */
	}
	break;
      case PTX_JOIN:
	if ( ignore_whitespace ) {
	  ic = cis_peek(in);
	  if ( isspace(ic) && ic != ps[1] )
	    goto failure; /* prevent the space from being ignored */
	}
     	break;
#ifndef NDEBUG
      default:
	input_error( in, EXS_FAIL, "Undefined aux op in template: %d\n", ec);
	break;
#endif
     } /* end switch ec */
     break;
   } /* end PT_AUX */

    case PT_QUOTE: /* take next character literally */
      pc = *++ps;
      /* and fall-through */
    default: {
again:
      ic = cis_peek(in);
      if ( ic != pc &&
	   ( !(local_options & MatchNoCase) || toupper(ic) != toupper(pc) ) ){
	if ( ignore_whitespace && isspace(ic) &&
	     ( !isident(pc) || !isident(cis_prevch(in)) ) ) {
	  (void)getch_marked(&marker);
	  goto again;
	}
	goto failure;
      }
      else (void)getch_marked(&marker);
      } /* end default */
    } /* end switch pc */
  continue_match: ;
  } /* end for pattern string */
 failure:
  match = FALSE;
  goto quit;
 success:
  match = TRUE;
 quit:
  if ( marker.marked ) {
    if ( match && ( options & MatchSwallow ) ) {
      if ( end_position_marked )
	cis_restore(in,&end_position);
      /* else leave the input stream at the end of the matched text */
      cis_release(in,&marker.start);
    }
    else {
      if ( end_position_marked )
	cis_release(in,&end_position);
      cis_restore(in,&marker.start); /* restore input to previous position */
    }
  }
  return match;
}

static int global_options;
Pattern current_rule = NULL;

static boolean
try_match( CIStream in, Pattern pat, COStream out, const unsigned char* goal )
{
  boolean result;
  varp varmark;
  CIStream args[MAX_ARG_NUM+1];

  args[0] = NULL;
  varmark = first_bound_var;
  if ( ! try_pattern( in, pat->pattern, &args[0], &args[0],
		      global_options, goal ) ) {
    if ( varmark != first_bound_var )
      prune_vars(varmark); /* undo variables bound within failed match */
    result = FALSE;
  }
  else {
    const unsigned char* as;
    enum Translation_Status save = translation_status;
    translation_status = Translate_Complete;
    /* pattern matches, so perform the specified action. */
    current_rule = pat;
    as = do_action( pat->action, args, out );
    assert( pat->action == NULL || *as == PT_END );
    result = TRUE;
    if ( translation_status == Translate_Complete )
      translation_status = save;
  }
  {
    CIStream * argp;
    for ( argp = &args[0] ; *argp != NULL ; argp++ )
      cis_close(*argp); /* de-allocate argument buffer */
  }
  return result;
}

static boolean
try_list( CIStream in, Patterns p, COStream out,
		  const unsigned char* goal ) {
  Pattern pat;
  for ( pat = p->head ; pat != NULL ; pat = pat->next ) {
    if ( translation_status != Translate_Complete )
      return translation_status != Translate_Failed;
    if ( try_match( in, pat, out, goal ) )
      return TRUE;
   }
  return FALSE;
}

static boolean
try_patterns( int ch, CIStream in, MarkBuf* start, Patterns p, COStream out,
		  const unsigned char* goal ) {
    MarkBuf mark;
    if ( p->dispatch != NULL ) {
      Patterns sub;
      sub = p->dispatch[ dispatch_index(ch) ];
      if ( sub != NULL ) {
	if ( sub->dispatch != NULL ) {
	  int xc;
	  if ( start == NULL ) {
	    start = &mark;
	    cis_mark(in, start);
	  }
	  xc = cis_getch(in);
	  assert ( ch == xc );
	  if ( try_patterns( cis_peek(in), in, start, sub, out, goal ) )
	    return TRUE;
	  else start = NULL;
	}
	else {
	  if ( start != NULL ) {
	    cis_restore(in, start);
	    start = NULL;
	  }
	  if ( try_list( in, sub, out, goal ) )
	    return TRUE;
	}
      }
    } /* end p->dispatch */
    if ( start != NULL ) {
      assert ( start != &mark );
      cis_restore(in, start);
    }
    return try_list( in, p, out, goal );
}

static int domains_checked = 1;

boolean translate ( CIStream in, Domain domainpt, COStream out,
		   const unsigned char* goal ){
  Domain idp;
  int ch, ch2;
  unsigned goal_char;
  enum Translation_Status save_fail;
  CIStream save_input;
  boolean no_match = TRUE;
  boolean discard = FALSE;

  save_input = input_stream;
  if ( save_input == NULL || cis_pathname(in) != NULL ||
       ( cis_is_file(in) && ! cis_is_file(save_input) ) )
    input_stream = in;
  for ( ; domains_checked < ndomains ; domains_checked++ ) {
    Domain dp = domains[domains_checked];
    if ( dp->patterns.head == NULL && dp->patterns.dispatch == NULL &&
  	 dp->name[0] != '\0' && dp->init_and_final_patterns == NULL )
      fprintf(stderr, "Domain name \"%s\" is referenced but not defined.\n",
		dp->name);
  }
  global_options = MatchSwallow;
  if ( line_mode )
    global_options |= MatchLine;
  if ( case_insensitive )
    global_options |= MatchNoCase;
  goal_char = goal == NULL ? ENDOP : get_goal_char(goal);
  save_fail = translation_status;
  translation_status = Translate_Complete;
  if ( discard_unmatched && domainpt->name[0] == '\0' )
    discard = TRUE;
  {	/* do any initialization actions */
  Pattern pat;
  for ( pat = domainpt->init_and_final_patterns ; pat != NULL ; pat = pat->next ) {
    const unsigned char* ps = pat->pattern;
    if ( ps[0] == PT_AUX && ps[2] == PT_END &&
  	 ( ps[1] == PTX_INIT ||
  	   ( ps[1] == PTX_BEGIN_FILE && goal == NULL && cis_is_file(in) &&
  		 cis_prevch(in) == EOF ) ) ) {
	current_rule = pat;
	do_action( pat->action, NULL, out );
	no_match = FALSE;
      }
    }
  }

  for ( ; translation_status == Translate_Complete ; ) {
    ch = cis_peek(in);
    if ( ch == EOF )
      break;  /* done */
    else if ( goal_char != ENDOP ) {
      if ( ch == goal_char ||
    	   (goal_char == UPOP(PT_SPACE) && isspace(ch) ) ) {
	if ( goal_char == goal[0] &&
	     (goal[1] == PT_END || goal[1] == PT_MATCH_ANY ||
	      goal[1] == PT_MATCH_ONE || goal[1] == PT_RECUR ) )
	  /* short-cut for single-character delimiter */
	  break;  /* done */
	else /* use general pattern matching */
	  if ( try_pattern( in, goal, NULL, NULL,
	     		    (global_options & ~MatchSwallow), goal ) )
	    break;
      }
    }
    for ( idp = domainpt ; idp != NULL ; idp = idp->inherits )
      if ( try_patterns( ch, in, NULL, &idp->patterns, out, goal ) ) {
	/* match found */
	if ( translation_status != Translate_Complete ) {
	  boolean result = translation_status != Translate_Failed;
	  translation_status = save_fail;
	  input_stream = save_input;
	  return result;
	}
	else goto next_char;
      }
    ch2 = cis_getch(in);
#ifndef NDEBUG
    if( ch2 != ch )
      input_error(in, EXS_INPUT, __FILE__
      " line %d : ch2 = '%c', ch = '%c'\n", __LINE__, ch2, ch);
#endif
    if ( !discard )
      cos_putch(out, (char)ch2);
next_char: ;
  }  /* end for */

  {		/* do any finalization actions */
  Pattern pat;
  for ( pat = domainpt->init_and_final_patterns ; pat != NULL ; pat = pat->next ) {
    const unsigned char* ps = pat->pattern;
    if ( ps[0] == PT_AUX && ps[2] == PT_END &&
	 ( ps[1] == PTX_FINAL ||
  	   ( ps[1] == PTX_END_FILE && goal == NULL && cis_is_file(in) &&
  	     cis_peek(in) == EOF ) ) ) {
      current_rule = pat;
      do_action( pat->action, NULL, out );
      no_match = FALSE;
     }
   }
  if ( no_match && cis_prevch(in) == EOF && cis_peek(in) == EOF ) {
    for ( pat = domainpt->patterns.head ; pat != NULL ; pat = pat->next ) {
      if ( pat->pattern[0] == PT_END ) {
	/* empty template matches empty input stream */
	current_rule = pat;
	do_action( pat->action, NULL, out );
	break;
      }
    }
  }
  }
  translation_status = save_fail;
  input_stream = save_input;
  return TRUE;  /* indicate success */
}
