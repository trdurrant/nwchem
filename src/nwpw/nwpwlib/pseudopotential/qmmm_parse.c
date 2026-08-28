#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "typesf2c.h"
#include "get_word.h"


#if (defined(CRAY) || defined(WIN32)) && !defined(__MINGW32__)
#define qmmm_parse_ QMMM_PARSE
#endif

/*
 * Bounds for the radial grid described by the <linear> section of the
 * pseudopotential file.  The built-in default is 2001 points, so the upper
 * limit here is ~500x larger than anything physically meaningful while still
 * capping the allocation (1e6 points = 8 MB per array).  At least two points
 * are needed for a grid with a spacing.
 */
#define QMMM_NRL_MIN 2
#define QMMM_NRL_MAX 1000000
#define QMMM_DRL_MAX 1.0e6

void FATR qmmm_parse_(Integer	*debug_ptr,
                      Integer	*lmax_ptr,
                      Integer	*locp_ptr,
                      double 	*rlocal_ptr,
                      char	sdir_name[],
                      Integer	*n9,
                      char	dir_name[],
                      Integer	*n0,
                      char	in_filename[],
                      Integer	*n1,
                      char	out_filename[],
                      Integer	*n2,
                      char	atom[],
                      Integer	*n3)
{
  int      debug;
  int      lmax_out,locp_out;
  double   rlocal_out;

  int      lmax;


    /*
     * Initialised because the get_*() calls below only warn ("NO Zion") on a
     * malformed <QMMM> section and then fall through; the values are used
     * unconditionally afterwards, which was an uninitialised read.  rc must
     * be non-zero: it appears in a divisor on the l'Hopital branch.
     */
    double   Zion = 0.0;      /* local psp parameters          */

    int      i,k,p;
    int      nrl;
    double       *rl,
    **psil,
    **pspl;
    double   drl,rmax;

    int      lmaxp,n_sigma = 0;
    double   rc = 1.0,rc1,rc2,rr1,rr2,ttt,sss,s_sigma;


    char   *w;
    FILE   *fp;

    char     comment[255];
    int      argc;



    int		m9 = ((int) (*n9));
    int		m0 = ((int) (*n0));
    int		m1 = ((int) (*n1));
    int		m2 = ((int) (*n2));
    int		m3 = ((int) (*n3));
    char *infile  = (char *) malloc(m9+m1+5);
    char *outfile = (char *) malloc(m0+m2+5);
    char *atom_out = (char *) malloc(m3+5);

    char *full_filename = (char *) malloc(m9+25+5);


    if (infile == ((char *) 0) || outfile == ((char *) 0) ||
        atom_out == ((char *) 0) || full_filename == ((char *) 0))
    {
        fprintf(stderr,"qmmm_parse: out of memory\n");
        exit(99);
    }

    debug = *debug_ptr;
    lmax_out   = *lmax_ptr;
    locp_out   = *locp_ptr;
    rlocal_out = *rlocal_ptr;

    (void) strncpy(infile, sdir_name, m9);
    infile[m9] = '\0';
    strcat(infile,"/");
    infile[m9+1] = '\0';
    strncat(infile,in_filename,m1);
    infile[m9+m1+1] = '\0';

    (void) strncpy(outfile, dir_name, m0);
    outfile[m0] = '\0';
    (void) strcat(outfile,"/");
    outfile[m0+1] = '\0';
    (void) strncat(outfile,out_filename,m2);
    outfile[m0+m2+1] = '\0';

    (void) strncpy(atom_out, atom, m3);
    atom_out[m3] = '\0';



    /* find the comment */
    strcpy(comment,"QMMM formatted  pseudopotential");
    fp = fopen(infile,"r+");
    if (fp == ((FILE *) 0))
    {
        fprintf(stderr,"qmmm_parse: cannot open %s\n",infile);
        exit(99);
    }
    w = get_word(fp);
    while ((w!=NIL) && (strcmp("<comment>",w)!=0))
        w = get_word(fp);

    if (w!=NIL)
    {
        /*
         * CWE-787 (adjacent to the reported CWE-129, same taint source):
         * the original loop walked a bare cursor along comment[255] with
         * strcpy() and no bound whatsoever, so a <comment> section longer
         * than 254 characters overflowed a stack buffer with file-controlled
         * bytes.  Append only what fits; keep consuming words either way so
         * the scan still stops at <end> exactly as before.
         */
        size_t used = 0;

        w = get_word(fp);
        while ((w!=NIL)&&(strcmp("<end>",w) != 0))
        {
            size_t lw = strlen(w);

            /* need lw chars, a separating blank, and the terminator */
            if (used + lw + 2 <= sizeof(comment))
            {
                memcpy(comment+used, w, lw);
                used += lw;
                comment[used++] = ' ';
                comment[used]   = '\0';
            }
            w = get_word(fp);
        }
    }
    fclose(fp);


    /* define linear grid */
    nrl  = 2001;
    rmax = 40.0;
    drl  = rmax/((double)(nrl-1));

    fp = fopen(infile,"r+");
    if (fp == ((FILE *) 0))
    {
        fprintf(stderr,"qmmm_parse: cannot open %s\n",infile);
        exit(99);
    }
    w = get_word(fp);
    while ((w != ((char *) EOF)) && (strcmp("<linear>",w) != 0))
        w = get_word(fp);
    if (w!=((char *) EOF))
    {
        int    nrl_in;
        double drl_in;

        /*
         * CWE-129 fix: nrl and drl come straight out of the pseudopotential
         * file and then size every allocation and bound every loop below
         * (rl[], pspl[0][], psil[0][]).  Two things went unchecked:
         *
         *  1. the return value of fscanf -- on a malformed <linear> line it
         *     leaves one or both variables untouched, so the code silently
         *     continued with a half-parsed grid;
         *  2. the range of nrl.  nrl <= 0 makes "malloc(nrl*sizeof(double))"
         *     return a 0/1-byte block (or, when negative, convert to a huge
         *     size_t and fail), while the very next statement writes rl[0]
         *     unconditionally -- outside any "i < nrl" loop, so none of the
         *     loop guards protect it.  A negative nrl also makes rmax
         *     nonsensical.
         *
         * Parse into temporaries and only commit once both values are sane.
         */
        if (fscanf(fp,"%d %lf",&nrl_in,&drl_in) != 2)
        {
            fprintf(stderr,
                "qmmm_parse: malformed <linear> section in %s\n",infile);
            fclose(fp);
            exit(99);
        }
        if (nrl_in < QMMM_NRL_MIN || nrl_in > QMMM_NRL_MAX)
        {
            fprintf(stderr,
                "qmmm_parse: <linear> grid size %d out of range [%d,%d] in %s\n",
                nrl_in, QMMM_NRL_MIN, QMMM_NRL_MAX, infile);
            fclose(fp);
            exit(99);
        }
        if (!(drl_in > 0.0) || drl_in > QMMM_DRL_MAX)
        {
            fprintf(stderr,
                "qmmm_parse: <linear> grid spacing %g out of range in %s\n",
                drl_in, infile);
            fclose(fp);
            exit(99);
        }
        nrl = nrl_in;
        drl = drl_in;
        rmax = ((double) (nrl-1))*drl;
    }
    fclose(fp);




    /* Read QMMM psp */
    fp = fopen(infile,"r+");
    if (fp == ((FILE *) 0))
    {
        fprintf(stderr,"qmmm_parse: cannot open %s\n",infile);
        exit(99);
    }
    w = get_word(fp);
    while ((w!=NIL) && (strcmp("<QMMM>",w)!=0))
        w = get_word(fp);

    /* Error occured */
    if (w==NIL)
    {
        printf("Error: <QMMM> section not found\n");
        fclose(fp);
        exit(99);
    }

    argc = to_eoln(fp);

    /*
     * CWE-787 fix: atom[] is a Fortran CHARACTER variable of length m3 (it
     * can be as short as two bytes), while the name being read comes from
     * the pseudopotential file.  The unbounded get_string() overflowed it;
     * get_string_n() clamps to the length Fortran actually passed us.
     */
    if (!get_string_n(fp,atom,(size_t) m3))  printf("NO Atom name\n");
    if (!get_float(fp,&Zion))  printf("NO Zion\n");
    if (!get_int(fp,&n_sigma)) printf("NO n_sigma\n");
    if (!get_float(fp,&rc))    printf("NO rc\n");
    fclose(fp);

    lmax  = 0;
    lmaxp = lmax+1;


    /* generate linear meshes */
    rl       = (double *) malloc(nrl*sizeof(double));
    psil     = (double **) malloc(lmaxp*sizeof(double*));
    pspl     = (double **) malloc(lmaxp*sizeof(double*));

    /*
     * CWE-129 fix (companion to the <linear> validation above): rl[0] is
     * written unconditionally, before any "i < nrl" loop, so a failed or
     * degenerate allocation is an immediate out-of-bounds write rather than
     * something the loop bounds would catch.
     */
    if (rl == ((double *) 0) || psil == ((double **) 0) ||
        pspl == ((double **) 0))
    {
        fprintf(stderr,"qmmm_parse: out of memory for %d grid points\n",nrl);
        exit(99);
    }

    rl[0] = 0.00004167;
    for (i=1; i<nrl; ++i)
    {
        rl[i] = drl*((double) i);
    }

    /* generate potential */
    rc1 = 1.0;
    for (i=0; i<n_sigma; ++i) rc1 *= rc;
    rc2 = rc1*rc;

    pspl[0] = (double *) malloc(nrl*sizeof(double));
    psil[0] = (double *) malloc(nrl*sizeof(double));
    if (pspl[0] == ((double *) 0) || psil[0] == ((double *) 0))
    {
        fprintf(stderr,"qmmm_parse: out of memory for %d grid points\n",nrl);
        exit(99);
    }
    if (Zion>0.0)
    {
        for (i=0; i<nrl; ++i)
        {
            rr1 = 1.0; for (p=0; p<n_sigma; ++p) rr1 *= rl[i];
            rr2 = rr1*rl[i];
            ttt = (rc1 - rr1);
            sss = (-rc2 - rr2);
            pspl[0][i] = -Zion*(ttt/sss);
            psil[0][i] = 0.0;
        }
    }
    else
    {
        for (i=0; i<nrl; ++i)
        {
            rr1 = 1.0; for (p=0; p<n_sigma; ++p) rr1 *= rl[i];
            rr2 = rr1*rl[i];
            ttt = (rc1 - rr1);
            sss = (rc2 - rr2);
            /* l'Hopital */
            if (fabs(sss)<1.0e-9)
                pspl[0][i] = (-Zion/rc) * ((double) n_sigma)/((double) (n_sigma+1));
            else
                pspl[0][i] = -Zion*(ttt/sss);
            psil[0][i] = 0.0;
        }

    }


    /* write outfile */
    fp = fopen(outfile,"w+");
    if (fp == ((FILE *) 0))
    {
        fprintf(stderr,"qmmm_parse: cannot write %s\n",outfile);
        exit(99);
    }
    fprintf(fp,"%s\n",atom_out);
    fprintf(fp,"%lf %lf %d   %d %d %lf\n",Zion,0.0,lmax,lmax_out,locp_out,rlocal_out);
    fprintf(fp,"%lf\n", rc);
    fprintf(fp,"%d %lf\n",nrl,drl);
    fprintf(fp,"%s\n",comment);

    /* appending pseudopotentials */
    for (k=0; k<nrl; ++k)
    {
        fprintf(fp,"%12.8lf", rl[k]);
        for (p=0; p<=lmax; ++p)
            fprintf(fp," %12.8lf", pspl[p][k]);
        fprintf(fp,"\n");
    }
    for (p=0; p<=lmax; ++p) free(pspl[p]);
    free(pspl);

    /* appending pseudowavefunctions */
    for (k=0; k<nrl; ++k)
    {
        fprintf(fp,"%12.8lf", rl[k]);
        for (p=0; p<=lmax; ++p)
            fprintf(fp," %12.8lf %12.8lf", psil[p][k],psil[p][k]);
        fprintf(fp,"\n");
    }
    for (p=0; p<=lmax; ++p) free(psil[p]);
    free(psil);

    fclose(fp);


    if (debug)
    {
        printf("QMMM pseudopotential Parameters\n\n");
        printf("atom : %s\n",atom);
        printf("Zion : %lf\n",Zion);
        printf(" lmax: %d\n",lmax);
        printf(" locp: %d\n",locp_out);
        printf(" rlocal: %lf\n\n",rlocal_out);

    }

    /* free malloc memory */
    free(rl);
    free(infile);
    free(outfile);
    free(full_filename);
    free(atom_out);

    fflush(stdout);
    return;

} /* main */




