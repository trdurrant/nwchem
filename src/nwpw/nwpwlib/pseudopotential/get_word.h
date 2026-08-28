/*
 $Id$
*/
#ifndef _GET_WORD_H_
#define _GET_WORD_H_
/* get_word.h -
   Author - Eric Bylaska

*/
#include	<stdio.h>
#include	<stddef.h>
#define	NIL	((char *) EOF)

extern char	*get_word(FILE *stream);
extern int 	get_line(FILE *, char *, int);
extern int 	to_eoln(FILE *);
extern int 	get_int(FILE *, int *);
extern int 	get_string(FILE *, char *);
/* Bounded form of get_string(): writes at most maxlen-1 characters plus a
   terminator into the caller's buffer.  Prefer this wherever the buffer
   size is known. */
extern int 	get_string_n(FILE *, char *, size_t);
extern int 	get_float(FILE *, double *);
extern int 	get_end(FILE *);
extern int 	remove_blanks(FILE *);


#endif
