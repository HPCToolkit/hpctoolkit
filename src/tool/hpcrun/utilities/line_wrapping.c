/*
  This code provides a minimal way to do line wrapping in C.
 
  strwrap() will take an input string and a desired line width and
  attempt to split it into suitable chunks.
 
  The result will be provided as an array of string pointers, each
  referring to the position of the first character on a line.
 
  It tries to be clever about whitespace and start each line at the
  beginning of a word (it will not erase whitespace in the middle of
  lines). Newlines encountered in the string will be converted into
  dummy lines of 0 width.
 
  How to use:
  - Prepare the text you wish to wrap
  - Call strwrap()
  - Print the result line by line
  - Free the arrays strwrap() returned
 
  The code was designed with the following restrictions:
  - the original string must remain unmodified
  - everything contained in a single function
  - no structs or declarations (also helps minimize cleanup)
 
  Realistic expectations: There is no support for wide characters. It
  has no notion of control codes or formatting markup and will treat
  these just like regular words. It doesn't really care about tabs. It
  doesn't do anything about line raggedness or hyphenation.
 
  This code is meant to be educational and hasn't been overly
  optimized. I could probably use char pointers instead of array
  offsets in many places. However, keeping it this way should make
  it easier to adapt for other languages.
 
  main() provides a small test program.
 
  Compile with something like: gcc strwrap.c -o strwraptest
 
  You may use this code in your program. Please don't distribute
  modified versions without clearly stating you have changed it.
 
  ulf.astrom@gmail.com / happyponyland.net, 2012-11-12
*/
 
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
 
 
/*
  Splits s into multiple lines of width w. Returns the number of
  lines. line_ret will be set to an array of string pointers, each
  pointing to the start of a line. len_ret will be set to an array of
  ints, describing how wide the corresponding line is. If an error
  occurs 0 is returned; line_ret and len_ret are then left unmodified.
*/
int strwrap(char * s, int w, char *** line_ret, int ** len_ret)
{
  int allocated; /* lines allocated */
  int lines; /* lines used */
  char ** line;
  int * len;
  int tl; /* total length of the string */
  int l; /* length of current line */
  int p; /* offset (from s) of current line */
  int close_word;
  int open_word;
  int i;
 
  if (s == NULL)
    return 0;
 
  tl = strlen(s);
 
  if (tl == 0 || w <= 0)
    return 0;
 
  lines = 0;
 
  /*
    Preemptively allocate memory. This should be enough for most uses;
    if we need more we will realloc later.
  */
  allocated = (tl / w) * 1.5 + 1;
 
  line = (char **) malloc(sizeof(char *) * allocated);
  len = (int *) malloc(sizeof(int) * allocated);
 
  if (line == NULL || len == NULL)
    return 0;
 
  /*
    p will be an offset from the start of the string and the start of
    the current line we are processing.
  */
 
  p = 0;
 
  while (p < tl)
  {
    /* Detect initial newlines */
    if (s[p] == '\n')
    {
      l = 0;
      goto make_new_line;
    }
 
    /*
      Fast-forward past initial whitespace on the current line. You
      might want to comment this out if you need formatting like
      "  * Bullet point lists" and wish to preserve the spaces.
    */
    if (isspace(s[p]))
    {
      p++;
      continue;
    }
 
    /*
      Decide how long the current line should be. We typically want
      the line to take up the full allowed line width, but we also
      want to limit the perceived length of the final line. If the
      line width overshoots the end of the string, truncate it.
    */
    if (p + w > tl)
      w = tl - p;
 
    l = w;
 
    /*
      If the break point ends up within a word, count how many
      characters of that word fall outside the window to the right.
    */
    close_word = 0;
 
    while (s[p + l + close_word] != '\0' && !isspace(s[p + l + close_word]))
      close_word++;
 
    /*
      Now backtrack from the break point until we find some
      whitespace. Keep track of how many characters we traverse.
    */
    open_word = 0;
 
    while (s[p + l] != '\0' && !isspace(s[p + l]))
    {
      l--;
      open_word ++;
 
      /*
	If the current word length is near the line width it will be
	hard to fit it all on a line, so we should just leave as much
	of it as possible on this line. Remove the fraction if you
	only want longer words to break.
      */
      if (open_word + close_word > w * 0.8)
      {
	l = w;
	break;
      }
    }
 
    /*
      We now have a line width we wish to use. Just make a final check
      there are no newlines in the middle of the line. If there are,
      break at that point instead.
    */
    for (i = 0; i < l; i++)
    {
      if (s[p + i] == '\n')
      {
	l = i;
	break;
      }
    }
 
  make_new_line:
    /*
      We have decided how long this line should be. Check that we have
      enough memory reserved for the line pointers; allocate more if
      needed.
    */
 
    line[lines] = &s[p];
    len[lines] = l;
    lines++;
 
    if (lines >= allocated)
    {
      allocated *= 2;
 
      line = (char **) realloc(line, sizeof(char *) * allocated);
      len = (int *) realloc(len, sizeof(int) * allocated);
 
      if (line == NULL || len == NULL)
	return 0;
    }
 
    /*
      Move on to the next line. This needs to be 1 less than the
      desired width or we will drop characters in the middle of
      really long words.
    */
    if (l == w)
      l--;
 
    p += l + 1;
  }
 
  /* Finally, relinquish memory we don't need */
  line = (char **) realloc(line, sizeof(char *) * lines);
  len = (int *) realloc(len, sizeof(int) * lines);
 
  *line_ret = line;
  *len_ret = len;
 
  return lines;
}
 
 
#if DBG_LINE_WRAPPING
 
/*
  Test program for strwrap.
*/
int main()
{
  char test[700];
  char shit[100];
  char ** line;
  int * len;
  int lines;
  int i;
  int w;
 
  /* Prepare the string we wish to wrap */
  strcpy(test, "Hey this is a test ok testing some more let's see where this line breaks just a couple words more andareallylongwordthatshouldbreaksomewhereinhalf to see it's really working    and    now    let's see    how     it     works with   lots  of   whitespace and a couple of\nnewlines and also\n\ndouble newlines in different arrangements \n\n andanotherreallylongwordrightafteranewlinesowecanseeit'sworkingallright and now let's see how it aligns with the full line width\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa and line width + 1\n aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab yeah it seems pretty ok now the last line should be 10 wide\nok thx bye");
 
  /* The width of the destination "box" */
  w = 33;
 
  /* Call strwrap like this to wrap the string */
  lines = strwrap(test, w, &line, &len);
 
  /*
    Print the result. We'll print the length of each line we received
    and pad each line to w width.
  */
  printf("Got %d lines:\n\n", lines);
 
  for (i = 0; i < lines; i++)
  {
    strncpy(shit, line[i], len[i]);
    shit[len[i]] = '\0';
    printf("%4d |%-*s|\n", len[i], w, shit);
  }
 
  /* This is the only cleanup needed. */
  free(line);
  free(len);
 
  return 0;
}
#endif // DBG_LINE_WRAPPING
