/* Re-use libbench2 and the test program, but override bench_main so that
   we can have different command-line syntax. */
#include "getopt.h"
#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fftw3.h>
#include <string.h>

#define CONCAT(prefix, name) prefix ## name
#if defined(BENCHFFT_SINGLE)
#define FFTW(x) CONCAT(fftwf_, x)
#elif defined(BENCHFFT_LDOUBLE)
#define FFTW(x) CONCAT(fftwl_, x)
#else
#define FFTW(x) CONCAT(fftw_, x)
#endif

/* from bench.c: */
extern unsigned the_flags;
extern int usewisdom;
extern int nthreads;

/* dummy routines to replace those in hook.c */
void install_hook(void) {}
void uninstall_hook(void) {}

int verbose;

static void do_problem(const char *param)
{
     bench_problem *p;
     if (verbose > 0)
	  printf("PLANNING PROBLEM: %s\n", param);
     p = problem_parse(param);
     BENCH_ASSERT(can_do(p));
     problem_alloc(p);
     setup(p);
     done(p);
     problem_destroy(p);
}

static void do_file(FILE *f)
{
     char s[1025];
     while (1 == fscanf(f, "%1024s", s))
	  do_problem(s);
}

static struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", optional_argument, 0, 'V'},
  {"verbose", optional_argument, 0, 'v'},

  {"canonical", no_argument, 0, 'c'},

  {"output-file", required_argument, 0, 'o'},

  {"impatient", no_argument, 0, 'i'},
  {"estimate", no_argument, 0, 'e'},
  {"exhaustive", no_argument, 0, 'x'},

  {"no-system-wisdom", no_argument, 0, 'n'},
  {"wisdom-file", required_argument, 0, 'w'},

  {"threads", required_argument, 0, 't'},

  /* options to restrict configuration to rdft-only, etcetera? */
  
  {0, no_argument, 0, 0}
};

static void help(FILE *f, const char *program_name)
{
     fprintf(
	  f, 
	  "Usage: %s [options] [sizes]\n"
"    Create wisdom (pre-planned/optimized transforms) for specified sizes,\n"
"    writing wisdom to stdout (or to a file, using -o).\n"
	  "\nOptions:\n"
 "                   -h, --help: print this help\n"
 "                -V, --version: print version/copyright info\n"
 "                -v, --verbose: verbose output\n"
 "              -c, --canonical: plan/optimize canonical set of sizes\n"
 "  -o FILE, --output-file=FILE: output to FILE instead of stdout\n"
 "              -i, --impatient: plan in IMPATIENT mode (PATIENT is default)\n"
 "               -e, --estimate: plan in ESTIMATE mode (not recommended)\n"
 "             -x, --exhaustive: plan in EXHAUSTIVE mode (may be slow)\n"
 "       -n, --no-system-wisdom: don't read /etc/fftw/ system wisdom file\n"
 "  -w FILE, --wisdom-file=FILE: read wisdom from FILE (stdin if -)\n"
#ifdef HAVE_THREADS
 "            -t N, --threads=N: plan with N threads\n"
#endif
	  "\nSize syntax: <type><inplace><direction><geometry>\n"
 "      <type> = c/r/k for complex/real(r2c,c2r)/r2r\n" 
 "   <inplace> = i/o for in/out-of place\n"
 " <direction> = f/b for forward/backward, omitted for k transforms\n"
 "  <geometry> = <n1>[x<n2>[x...]], e.g. 10x12x14\n"
 "               -- for k transforms, after each dimension is a <kind>:\n"
 "                     <kind> = f/b/h/e00/e01/e10/e11/o00/o01/o10/o11\n"
 "                              for R2HC/HC2R/DHT/REDFT00/.../RODFT11\n"
	  , program_name);
}

/* powers of two and ten up to 2^20, for now */
static char canonical_sizes[][32] = {
     "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024",
     "2048", "4096", "8192", "16384", "32768", "65536", "131072",
     "262144", "524288", "1048576",

     "10", "100", "1000", "10000", "100000", "1000000",

     "2x2", "4x4", "8x8", "10x10", "16x16", "32x32", "64x64", "100x100",
     "128x128", "512x512", "1000x1000", "1024x1024",

     "2x2x2", "4x4x4", "8x8x8", "10x10x10", "16x16x16", "32x32x32",
     "64x64x64", "100x100x100"
};

#define NELEM(array)(sizeof(array) / sizeof((array)[0]))

int bench_main(int argc, char *argv[])
{
     int c;
     int ind;
     unsigned i;
     char *short_options = make_short_options(long_options);
     int impatient = 0;
     int system_wisdom = 1;
     int canonical = 0;
     FILE *output_file;
     char *output_fname = 0;

     verbose = 0;
     usewisdom = 0;

     bench_srand(1);
#ifdef HAVE_THREADS
     BENCH_ASSERT(FFTW(init_threads)());
#endif

     while ((c = getopt_long (argc, argv, short_options,
			      long_options, &ind)) != -1) {
	  switch (c) {
	      case 'h':
		   help(stdout, argv[0]);
		   exit(EXIT_SUCCESS);
		   break;

	      case 'V':
		   printf("fftw-wisdom tool for FFTW version " VERSION ".\n");
		   printf(
"\n"
"Copyright (c) 2002 Matteo Frigo\n"
"Copyright (c) 2002 Steven G. Johnson\n"
"\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
			);
		   exit(EXIT_SUCCESS);
		   break;
		   
	      case 'v':
		   if (optarg)
			verbose = atoi(optarg);
		   else
			++verbose;
		   break;
		   
	      case 'c':
		   canonical = 1;
		   break;

	      case 'o':
		   if (output_fname)
			bench_free(output_fname);
		   
		   if (!strcmp(optarg, "-"))
			output_fname = 0;
		   else {
			output_fname = bench_malloc(sizeof(char) *
						    (strlen(optarg) + 1));
			strcpy(output_fname, optarg);
		   }
		   break;

	      case 'i':
		   impatient = 1;
		   break;

	      case 'e':
		   the_flags |= FFTW_ESTIMATE;
		   break;

	      case 'x':
		   the_flags |= FFTW_EXHAUSTIVE;
		   break;

	      case 'n':
		   system_wisdom = 0;
		   break;

	      case 'w': {
		   FILE *w = stdin;
		   if (strcmp(optarg, "-") && !(w = fopen(optarg, "r"))) {
			fprintf(stderr,
				"fftw-wisdom: error opening \"%s\"", optarg);
			perror("");
			exit(EXIT_FAILURE);
		   }
		   if (!FFTW(import_wisdom_from_file)(w)) {
			fprintf(stderr, "fftw_wisdom: error reading wisdom"
				"from \"%s\"\n", optarg);
			exit(EXIT_FAILURE);
		   }
		   if (w != stdin)
			fclose(w);
		   break;
	      }

	      case 't':
		   nthreads = atoi(optarg);
#ifndef HAVE_THREADS
		   if (nthreads > 1) {
			fprintf(stderr, "fftw-wisdom: "
				"not compiled with thread support\n");
			exit(EXIT_FAILURE);
		   }
#endif
		   break;

	      case '?':
		   /* `getopt_long' already printed an error message. */
		   break;

	      default:
		   abort ();
	  }
     }

     if (!impatient)
	  the_flags |= FFTW_PATIENT;

     if (system_wisdom)
	  if (!FFTW(import_system_wisdom)() && verbose)
	       fprintf(stderr, "fftw-wisdom: system-wisdom import failed\n");

     if (canonical) {
	  for (i = 0; i < NELEM(canonical_sizes); ++i) {
	       unsigned j;
	       char types[][8] = { 
		    "cof", "cob", "cif", "cib", "rof", "rob", "rif", "rib"
	       };
	       
	       for (j = 0; j < NELEM(types); ++j) {
		    char ps[64];
		    if (!strchr(canonical_sizes[i],'x')
			|| !strchr(types[j],'o')) {
			 sprintf(ps, "%s%s", types[j], canonical_sizes[i]);
			 do_problem(ps);
		    }
	       }
	  }
     }

     while (optind < argc) {
	  if (!strcmp(argv[optind], "-"))
	       do_file(stdin);
	  else
	       do_problem(argv[optind]);
	  ++optind;
     }
     
     if (!output_fname)
	  output_file = stdout;
     else
	  if (!(output_file = fopen(output_fname, "w"))) {
	       fprintf(stderr,
		       "fftw-wisdom: error creating \"%s\"", output_fname);
	       perror("");
	       exit(EXIT_FAILURE);
	  }

     FFTW(export_wisdom_to_file)(output_file);
     if (output_file != stdout)
	  fclose(output_file);
     if (output_fname)
	  bench_free(output_fname);

     cleanup();
     bench_free(short_options);

     return EXIT_SUCCESS;
}
