/*
 * regex - Regular expression pattern matching
 *         and replacement
 *
 * By:  Ozan S. Yigit (oz), Dept. of Computer Science, York University
 * Mods: Craig Durland
 *
 * These routines are the PUBLIC DOMAIN equivalents of regex routines as
 * found in 4.nBSD UN*X, with minor extensions.
 *
 * These routines are derived from various implementations found in software
 * tools books, and Conroy's grep.  They are NOT derived from
 * licensed/restricted software.  For more interesting/academic/complicated
 * implementations, see Henry Spencer's regexp routines, or GNU Emacs
 * pattern matching module.
 *
 * dfa = deterministic finite automata
 * Routines:
 *  re_comp: compile a regular expression into a DFA.
 *	char *re_comp(s)
 *	char *s;
 *	returns: NULL if OK, else error string
 *	If s is NULL or 0 length, last compiled pattern is used.
 *  re_exec: execute the DFA to match a pattern.
 *	int re_exec(s)
 *	char *s;
 *  re_subs: substitute the matched portions in a new string.
 *	int re_subs(src, dst)
 *	char *src;
 *	char *dst;
 *  re_fail:	failure routine for re_exec.
 *	void re_fail(msg, op)
 *	char *msg;
 *	char op;
 *  
 * Regular Expressions:
 *
 *      [1]     char    matches itself, unless it is a special
 *                      character (metachar): . \ [ ] * + ^ $
 *
 *      [2]     .       matches any character.
 *
 *      [3]     \       matches the character following it, except
 *			when followed by one of: ()123456789<> adnwW
 *			(see [7], [8] and [9])
 *			It is used as an escape character for all other
 *			meta-characters, and itself.  When used in a set
 *			([4]), it is treated as an ordinary character.
 *
 *      [4]     [set]   matches one of the characters in the set.
 *                      If the first character in the set is "^",
 *                      it matches a character NOT in the set. A
 *                      shorthand S-E is used to specify a set of
 *                      characters S upto E, inclusive. The special
 *                      characters "]" and "-" have no special
 *                      meaning if they appear as the first chars
 *                      in the set.
 *                      examples:        match:
 *
 *                              [a-z]    any lowercase alpha
 *
 *                              [^]-]    any char except ] and -
 *
 *                              [^A-Z]   any char except uppercase
 *                                       alpha
 *
 *                              [a-zA-Z] any alpha
 *
 *      [5]     *       any regular expression form [1] to [4], followed by
 *                      closure char (*) matches zero or more matches of
 *                      that form.
 *
 *      [6]     +       same as [5], except it matches one or more.
 *
 *      [7]             a regular expression in the form [1] to [10], enclosed
 *                      as \(form\) matches what form matches. The enclosure
 *                      creates a set of tags, used for [8] and for
 *                      pattern substution. The tagged forms are numbered
 *			starting from 1.
 *
 *      [8]             a \ followed by a digit 1 to 9 matches whatever a
 *                      previously tagged regular expression ([7]) matched.
 *
 *	[9]	\<	a regular expression starting with a \< construct
 *		\>	and/or ending with a \> construct, restricts the
 *			pattern matching to the beginning of a word, and/or
 *			the end of a word. A word is defined to be a character
 *			string beginning and/or ending with the characters
 *			A-Z a-z 0-9 and _. It must also be preceded and/or
 *			followed by any character outside those mentioned.
 *
 *      [10]            a composite regular expression xy where x and y
 *                      are in the form [1] to [10] matches the longest
 *                      match of x followed by a match for y.
 *
 *      [11]	^	a regular expression starting with a ^ character
 *		$	and/or ending with a $ character, restricts the
 *                      pattern matching to the beginning of the line,
 *                      or the end of line. [anchors] Elsewhere in the
 *			pattern, ^ and $ are treated as ordinary characters.
 *
 * Acknowledgements:
 *   HCR's Hugh Redelmeier has been most helpful in various stages of
 *   development.  He convinced me to include BOW and EOW constructs,
 *   originally invented by Rob Pike at the University of Toronto.
 * References:
 *   Software tools		Kernighan & Plauger
 *   Software tools in Pascal	Kernighan & Plauger
 *   Grep [rsx-11 C dist]	David Conroy
 *   ed - text editor		Un*x Programmer's Manual
 *   Advanced editing on Un*x	B. W. Kernighan
 *   RegExp routines		Henry Spencer
 * Notes:
 *  This implementation uses a bit-set representation for character sets for
 *    speed and compactness.  Each character is represented by one bit in a
 *    128-bit block.  Thus, SET or NSET always takes a constant 16 bytes in
 *    the internal dfa, and re_exec does a single bit comparison to locate
 *    the character in the set.
 *  Put CLO in front of what gets closed for ease of interpreting.
 *  Put END at end of what gets closed to limit recursion.
 * Examples:
 *	pattern:	foo*.*
 *	compile:	CHR f CHR o CLO CHR o END CLO ANY END END
 *	matches:	fo foo fooo foobar fobar foxx ...
 *
 *	pattern:	fo[ob]a[rz]	
 *	compile:	CHR f CHR o SET bitset CHR a SET bitset END
 *	matches:	fobar fooar fobaz fooaz
 *
 *	pattern:	foo\\+
 *	compile:	CHR f CHR o CHR o CHR \ CLO CHR \ END END
 *	matches:	foo\ foo\\ foo\\\  ...
 *
 *	pattern:	\(foo\)[1-3]\1	(same as foo[1-3]foo)
 *	compile:	BOT 1 CHR f CHR o CHR o EOT 1 SET bitset REF 1 END
 *	matches:	foo1foo foo2foo foo3foo
 *
 *	pattern:	\(fo.*\)-\1
 *	compile:	BOT 1 CHR f CHR o CLO ANY END EOT 1 CHR - REF 1 END
 *	matches:	foo-foo fo-fo fob-fob foobar-foobar ...
 */

/* $Id$ */

/**
 * $Log$
 * Revision 1.1  1993/01/16 20:49:12  hbs
 * Adding normalize-path code to utilities directory/module.
 *
 * Revision 1.1  1993/01/16  20:21:40  hbs
 * Committing directory normalize-path and contents.
 *
 * Revision 100.1  1992/11/11  06:23:45  devin
 * renamed regexp reg-expr to avoid conflicts with the predefined regular
 * expressions
 *
 * Revision 100.1  1992/11/09  23:59:12  devin
 * added regexps
 *
 * Initial revision
**/

#ifndef REGEXP
#define REGEXP

#ifdef __cplusplus
extern "C" {
#endif

/* regexp_comp(): compile a regular expression into a DFA. 
    returns: NULL if OK, else error string
    If s is NULL or 0 length, last compiled pattern is used.
*/
unsigned char*
regexp_comp (unsigned char* s, unsigned char* automaton, int bufsize);

/* re_exec: execute the DFA to match a pattern. 
*/
int 
regexp_exec (unsigned char* s, int SoL, int move, unsigned char* automaton);

/* re_subs: substitute the matched portions in a new string. 
*/
int 
regexp_subs (unsigned char *src, unsigned char *dst);

/* re_fail:	failure routine for re_exec. 
*/
void 
regexp_fail (unsigned char *msg, unsigned char op);

/* beginning of pattern and end of pattern contanin the start and end position
   of the the matched pieces.  
   bopat [0], eopat [0] is the total mathc
   bopat [1-9] .. is the part matched by \1-9
*/
extern unsigned char *bopat[], *eopat[];

/* error string returned if the dfa argument to regexp_comp() is too short */
#define DFA_SHORT "The buffer provided to store the compiled DFA is too small."

#ifdef __cplusplus
}
#endif

#endif /* REGEXP */
