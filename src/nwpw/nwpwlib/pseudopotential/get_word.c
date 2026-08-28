/*
 $Id$
   get_word.c -
   Author - Eric Bylaska

*/
#include	<stdio.h>
#include	<string.h>
#include	"get_word.h"


/*
 * CWE-787 fix: every routine below accumulates a whitespace-delimited token
 * into this one static buffer, and none of them bounded the write.  Any token
 * of 72 characters or more -- an over-long atom name, a path, a stray run of
 * characters in a comment -- overflowed it.
 *
 * The buffer size is deliberately left at 72.  get_string() copies the token
 * out into a caller-supplied buffer, so enlarging this array would raise the
 * maximum number of bytes handed to every existing caller and could turn a
 * latent overflow in one of them into a live one.  Bounding is the fix here,
 * not growing.
 *
 * Tokens longer than the limit are truncated, but the remainder of the token
 * is still consumed from the stream, so a long token stays one token and the
 * caller's view of the token sequence is unchanged.
 */
#define	WORDSIZE	72

static	char	word[WORDSIZE];

/* Read one whitespace-delimited token into buf, truncating at maxlen-1
   characters but consuming the whole token.  Returns the number of
   characters stored, or -1 if end-of-file was hit before any token. */
static int read_token(FILE *stream, char *buf, size_t maxlen)
{
    int    c;
    size_t n = 0;

    /* skip leading whitespace, exactly as "%s" does */
    do {
        c = fgetc(stream);
        if (c == EOF)
            return (-1);
    } while (c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\f' || c=='\v');

    while (c != EOF &&
           c!=' ' && c!='\t' && c!='\n' && c!='\r' && c!='\f' && c!='\v')
    {
        if (n < maxlen-1)
            buf[n++] = (char) c;
        c = fgetc(stream);
    }
    if (c != EOF)
        ungetc(c,stream);

    buf[n] = '\0';
    return ((int) n);
}

char	*get_word(FILE *stream)
{
    if (read_token(stream,word,sizeof(word)) >= 0)
        return word;
    else
        return NIL;
}


int get_line(stream,line,maxlen)
FILE *stream;
char *line;
int  maxlen;
{
    int   c,i;

    /* c is tested after the loop; it is never assigned when maxlen <= 1,
       so give it a defined value. */
    c = 0;
    for (i=0; i<(maxlen-1) && (c=fgetc(stream))!=EOF && c!='\n'; ++i)
        line[i] = c;

    if (c=='\n')
    {
        line[i] = c;
        ++i;
    }
    line[i] = '\0';
    return i;
}


int to_eoln(stream)
FILE *stream;
{
    int   c,i;

    i = 0;
    while ((c=fgetc(stream))!=EOF && c!='\n')
        ++i;

    return i;
}


int     get_int(FILE *stream, int *ii)
{
    int c,value,n,i;

    for (i=0; i<(int) sizeof(word); ++i) word[i] = '\0';
    value = remove_blanks(stream);

    value = 0;
    n     = 0;
    while ((c=fgetc(stream))!=EOF && c!='\n' && c!=' ' && c!='\t')
    {
        /* CWE-787 fix: bound the store; keep consuming the rest of the
           token so the stream position is unchanged. */
        if (n < (int) sizeof(word) - 1)
        {
            word[n] = c;
            ++n;
        }
    }
    word[n] = '\0';
    if ((c=='\n') || (c==' ') || (c=='\t'))
    {
        ungetc(c,stream);
        --n;
    }

    value = sscanf(word,"%d",ii);
    if (!value) for (i=0; i<n; ++i) ungetc(word[n-i],stream);


    return value;
}




int get_float(FILE *stream, double *ff)
{
    int c,value,n,i;

    for (i=0; i<(int) sizeof(word); ++i) word[i] = '\0';
    value = remove_blanks(stream);

    value = 0;
    n     = 0;
    while ((c=fgetc(stream))!=EOF && c!='\n' && c!=' ' && c!='\t')
    {
        /* CWE-787 fix: bound the store; keep consuming the rest of the
           token so the stream position is unchanged. */
        if (n < (int) sizeof(word) - 1)
        {
            word[n] = c;
            ++n;
        }
    }
    word[n] = '\0';
    if ((c=='\n') || (c==' ') || (c=='\t'))
    {
        ungetc(c,stream);
        --n;
    }

    value = sscanf(word,"%lf",ff);
    if (!value) for (i=0; i<n; ++i) ungetc(word[n-i],stream);


    return value;
}





/*
 * get_string() writes a file-controlled token into a caller-supplied buffer
 * whose size it is never told, via an unbounded sscanf "%s".  Callers that
 * pass a small buffer -- qmmm_parse_() hands it a Fortran CHARACTER variable
 * that can be as short as two bytes -- are overflowed by any longer name in
 * the input file.
 *
 * The signature cannot be changed without touching every caller in the tree,
 * so add a bounded variant and leave get_string() as a compatibility wrapper
 * with its historical 72-byte behaviour.  Callers that know their buffer size
 * should move to get_string_n().
 */
int     get_string_n(FILE *stream, char *string, size_t maxlen)
{
    int c,value,n,i;

    if (string == ((char *) 0) || maxlen == 0)
        return 0;

    for (i=0; i<(int) sizeof(word); ++i) word[i] = ' ';
    value = remove_blanks(stream);

    n     = 0;
    while ((c=fgetc(stream))!=EOF && c!='\n' && c!=' ' && c!='\t')
    {
        /* CWE-787 fix: bound the store; keep consuming the rest of the
           token so the stream position is unchanged. */
        if (n < (int) sizeof(word) - 1)
        {
            word[n] = c;
            ++n;
        }
    }
    word[n] = '\0';

    if ((c=='\n'))
    {
        ungetc(c,stream);
        --n;
    }

    /* Copy out, never exceeding the caller's buffer. */
    value = 0;
    if (word[0] != '\0')
    {
        size_t len = strlen(word);

        if (len > maxlen-1)
            len = maxlen-1;
        memcpy(string, word, len);
        string[len] = '\0';
        value = 1;
    }
    if (!value) for (i=0; i<n; ++i) ungetc(word[n-i],stream);


    return value;
}


int     get_string(FILE *stream, char *string)
{
    /* Historical behaviour: at most sizeof(word)-1 characters plus a
       terminator are ever produced, so that is the implied bound. */
    return get_string_n(stream, string, sizeof(word));
}


int remove_blanks(FILE *stream)
{
    int c,n=0,value;

    while ((c=fgetc(stream))!=EOF && c!='\n' && (c==' ' || c=='\t'))
        ++n;

    ungetc(c,stream);


    value = 1;
    if ((c=='\n'))
    {
        value = 0;
        --n;
    }

    return value;
}


int     get_end(FILE *stream)
{
    int c,value,n,i;

    for (i=0; i<(int) sizeof(word); ++i) word[i] = '\0';
    value = remove_blanks(stream);

    value = 0;
    n     = 0;
    while ((c=fgetc(stream))!=EOF && c!='\n' && c!=' ' && c!='\t')
    {
        /* CWE-787 fix: bound the store; keep consuming the rest of the
           token so the stream position is unchanged. */
        if (n < (int) sizeof(word) - 1)
        {
            word[n] = c;
            ++n;
        }
    }
    word[n] = '\0';
    if ((c=='\n') || (c==' ') || (c=='\t'))
    {
        ungetc(c,stream);
        --n;
    }
    for (i=0; i<n; ++i) ungetc(word[n-i],stream);

    value = !strcmp("<end>",word);


    return value;
}

