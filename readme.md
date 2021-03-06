## Since this covering this I have run it through valgrind and the depths of hell have opened up. GEMA seems to be completely full of use after free errors. Some of these likely constitute security vulnerabilities. I will leave this repository up because I still think the language is nice but I do not reccomend using it for anything other than experimentation with data you trust as input.

# General Purpose Text Processor

rewrite is an import of the original gema repository. gema the source code has not been updated since 2004, but despite this, it just seems to work!

gema is a general purpose text processing utility based on the concept of pattern matching. In general, it reads an input file and copies it to an output file, while performing certain transformations to the data as specified by a set of patterns defined by the user. It can be used to do the sorts of things that are done by Unix utilities such as `cpp`, `grep`, `sed`, `awk`, or strings. It can be used as a macro processor, but it is much more general than `cpp` or `m4` because it does not impose any particular syntax for what a macro call looks like. Unlike utilities like `sed` or `awk`, gema can deal with patterns that span multiple lines and with nested constructs. It is also distinguished by being able to use multiple sets of rules to be used in different contexts.

Homepage: https://gema.sourceforge.net/new/index.shtml  
Homepage (archived): https://web.archive.org/web/2021*/http://gema.sourceforge.net/new/index.shtml  

### Quickstart

```sh
cd ./src
sh gemabuild
./gema
```

### Install

```sh
cd ./src
sh gemabuild
sudo sh gemabuild install
```

### Documentation

```sh
cd ./doc
make
```

## Original Readme

This directory contains "gema", the general-purpose macro processor.
The sub-directories are:

   src        source program (in ANSI C)
   doc        documentation, including Unix "man" page and user manual.
   examples   sample applications, including LaTeX to HTML conversion.
   test       script and data for verification of build

This description applies to version 1.4.

The program currently works on several varieties of Unix, MS-DOS,
Microsoft Windows, and Macintosh (both OS-X and earlier MACOS with
MPW). It may need some modification for use on other systems, particular
in regard to pathname syntax and accessing file attributes; see the
places where ``#ifdef unix'', ``#ifdef MSDOS'', and ``#ifdef MACOS'' are
used.

On most systems, you can build the program by just cd-ing to the "src"
directory and then running the command "gemabuild".  This will build and
test the program, automatically selecting the appropriate compiler
options for any of many common operating systems.  (There are actually
two script files, "gemabuild" for Unix and "gemabuild.bat" for Windows.)

## Further build details if needed

To build the program, you just need to use an ANSI C compiler to
compile all of the ".c" files and link the resulting object files
together.  An example "Makefile" for Unix is provided.  Normally you
should compile with optimization and with the option "-DNDEBUG" to omit
internal debugging checks.  You may also want to use "-DTRACE" to
enable the experimental diagnostic trace option.

For MS-DOS (16-bit mode), there is a "gema.mak" file from Microsoft
Quick C.  It builds using the "small memory model".  You'll need to
specify a stack size somewhere in the range of 4000 to 6000 bytes; I'm
currently using 5600.

After building the program, you can verify that it works correctly by
running the command "gematest" in the "test" directory (script file
"gematest" for Unix or "gematest.bat" for Windows.) The test passes if
no file comparison differences are reported.

The "examples" directory includes the following non-trivial pattern files
for document conversion:

    latex.dat      convert LaTeX to HTML
    man-html.dat   convert "man" page from "nroff" to HTML
    ht.dat         convert HTML to LaTeX
    tex.dat        used with "latex.dat" for low-level TeX features

The script "latex.sh" can be used for LaTeX to HTML conversion after
modifying it for the actual location of the files used.  It takes care of
automatically running the program a second time when needed to resolve
forward references. 

These conversion pattern files are not complete; they have been
sufficient for the documents I have been working with, but you may find
additional features that you need.  The conversion provides a clear
warning about what is not recognized, and extending the patterns is easy.

Also in the "examples" directory is file "c2dyl.dat", which reads C
source files and does a crude preliminary syntax conversion to the new
Dylan programming language.  Even if you aren't interested in Dylan, this
file serves as an example of how recursive pattern matching can be used
to parse infix expressions; note how the rules look very similar to a BNF
description of the expression syntax.


"gema" is not a commercial product; it has been donated to the public
domain in order to maximize its usefulness.  Let me know if you find any
bugs, but I don't promise to fix them.  If you fix something or extend
the program yourself, please send me your improvements.  Also send me
e-mail if you would like to be informed of any future updates.

    David N. Gray

    e-mail:  DGray@acm.org

    175 Dalma Drive
    Mountain View, CA  94041

    February, 2002
