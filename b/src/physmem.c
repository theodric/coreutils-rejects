/* physmem - report the total/available/recommended memory
   Copyright (C) 2009-2012 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Assaf Gordon.  */

#include <config.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>

#include "system.h"
#include "error.h"
#include "xstrtol.h"
#include "physmem.h"
#include "human.h"

#ifndef RLIMIT_DATA
struct rlimit { size_t rlim_cur; };
# define getrlimit(Resource, Rlp) (-1)
#endif

/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "physmem"

#define AUTHORS proper_name ("Assaf Gordon")

/* Human-readable options for output.  */
static int human_output_opts;

enum memory_report_type
  {
    total,			/* default */
    available,
    recommended
  };

static enum memory_report_type memory_report_type = total;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  HUMAN_SI_OPTION= CHAR_MAX + 1
};

static struct option const longopts[] =
{
  {"total", no_argument, NULL, 't'},
  {"available", no_argument, NULL, 'a'},
  {"recommended", no_argument, NULL, 'r'},
  {"human", no_argument, NULL, 'h'},
  {"si", no_argument, NULL, HUMAN_SI_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

/* Return the default sort size.
   FIXME: this function was copied from sort.c .
          extract it to a separate module.
 */
static size_t
default_sort_size (void)
{
  /* Let SIZE be MEM, but no more than the maximum object size,
     total memory, or system resource limits.  Don't bother to check
     for values like RLIM_INFINITY since in practice they are not much
     less than SIZE_MAX.  */
  size_t size = SIZE_MAX;
  struct rlimit rlimit;
  if (getrlimit (RLIMIT_DATA, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#ifdef RLIMIT_AS
  if (getrlimit (RLIMIT_AS, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#endif

  /* Leave a large safety margin for the above limits, as failure can
     occur when they are exceeded.  */
  size /= 2;

#ifdef RLIMIT_RSS
  /* Leave a 1/16 margin for RSS to leave room for code, stack, etc.
     Exceeding RSS is not fatal, but can be quite slow.  */
  if (getrlimit (RLIMIT_RSS, &rlimit) == 0 && rlimit.rlim_cur / 16 * 15 < size)
    size = rlimit.rlim_cur / 16 * 15;
#endif

  /* Let MEM be available memory or 1/8 of total memory, whichever
     is greater.  */
  double avail = physmem_available ();
  double total = physmem_total ();
  double mem = MAX (avail, total / 8);

  /* Leave a 1/4 margin for physical memory.  */
  if (total * 0.75 < size)
    size = total * 0.75;

  /* Return the minimum of MEM and SIZE, but no less than
     MIN_SORT_SIZE.  Avoid the MIN macro here, as it is not quite
     right when only one argument is floating point.  */
  if (mem < size)
    size = mem;
  return size;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]...\n"), program_name);
      fputs (_("\
Prints information about physical memory.\n\
\n\
"), stdout);
      fputs (_("\
  -t, --total           print the total physical memory.\n\
  -a, --available       print the available physical memory.\n\
  -r, --recommended     print a safe recommended amount of useable memory.\n\
  -h, --human-readable  print sizes in human readable format (e.g., 1K 234M 2G)\n\
      --si              like -h, but use powers of 1000 not 1024\n\
"), stdout);

      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  size_t memory =0 ;
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while (1)
    {
      int c = getopt_long (argc, argv, "thar", longopts, NULL);
      if (c == -1)
        break;
      switch (c)
        {
        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        case 't':
          memory_report_type = total;
          break;

        case 'a':
          memory_report_type = available;
          break;

        case 'r':
          memory_report_type = recommended;
          break;

        case 'h':
          human_output_opts = human_autoscale | human_SI | human_base_1024;
          break;

        case HUMAN_SI_OPTION:
          human_output_opts = human_autoscale | human_SI;
          break;

        default:
          usage (EXIT_FAILURE);
        }
    }

  switch(memory_report_type)
    {
    case total:
      memory = physmem_total();
      break;

    case available:
      memory = physmem_available();
      break;

    case recommended:
      memory = default_sort_size();
      break;
    }

  char buf[LONGEST_HUMAN_READABLE + 1];
  fputs (human_readable (memory, buf, human_output_opts,1,1),stdout);
  fputs("\n", stdout);

  exit (EXIT_SUCCESS);
}
