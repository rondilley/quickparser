/****
 *
 * $Id: main.c,v 1.6 2011/08/18 15:24:20 rdilley Exp $
 *
 * Copyright (c) 2011, Ron Dilley
 * All rights reserved.
 *
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*   - Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   - Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.
*   - Neither the name of Uberadmin/BaraCUDA/Nightingale nor the names of
*     its contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
****/

/****
 *
 * includes
 *.
 ****/

#include "main.h"

/****
 *
 * local variables
 *
 ****/

PRIVATE char *cvsid = "$Id: main.c,v 1.6 2011/08/18 15:24:20 rdilley Exp $";

/****
 *
 * global variables
 *
 ****/

PUBLIC int quit = FALSE;
PUBLIC int reload = FALSE;
PUBLIC Config_t *config = NULL;

/****
 *
 * external variables
 *
 ****/

extern int errno;
extern char **environ;

/****
 *
 * main function
 *
 ****/

int main(int argc, char *argv[]) {
  FILE *inFile = NULL, *outFile = NULL;
  char inBuf[8192];
  char outFileName[PATH_MAX];
  char patternBuf[4096];
  char oBuf[4096];
  PRIVATE int c = 0, i, ret;
  int lineCount = 0;

#ifndef DEBUG
  struct rlimit rlim;

  rlim.rlim_cur = rlim.rlim_max = 0;
  setrlimit( RLIMIT_CORE, &rlim );
#endif

  /* setup config */
  config = ( Config_t * )XMALLOC( sizeof( Config_t ) );
  XMEMSET( config, 0, sizeof( Config_t ) );

  /* force mode to forground */
  config->mode = MODE_INTERACTIVE;

  /* store current pid */
  config->cur_pid = getpid();

  /* get real uid and gid in prep for priv drop */
  config->gid = getgid();
  config->uid = getuid();

  while (1) {
    int this_option_optind = optind ? optind : 1;
#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    static struct option long_options[] = {
      {"config", required_argument, 0, 'c' },
      {"version", no_argument, 0, 'v' },
      {"debug", required_argument, 0, 'd' },
      {"help", no_argument, 0, 'h' },
      {0, no_argument, 0, 0}
    };
    c = getopt_long(argc, argv, "vd:hc:", long_options, &option_index);
#else
    c = getopt( argc, argv, "vd:hc:" );
#endif

    if (c EQ -1)
      break;

    switch (c) {

    case 'c':
      /* load config */
      if ( loadConfig( optarg ) != TRUE ) {
	fprintf( stderr, "ERR - Problem while loading config\n" );
	return( EXIT_FAILURE );
      }
      break;

    case 'v':
      /* show the version */
      print_version();
      return( EXIT_SUCCESS );

    case 'd':
      /* show debig info */
      config->debug = atoi( optarg );
      break;

    case 'h':
      /* show help info */
      print_help();
      return( EXIT_SUCCESS );

    default:
      fprintf( stderr, "Unknown option code [0%o]\n", c);
    }
  }

  /* check dirs and files for danger */

  if ( time( &config->current_time ) EQ -1 ) {
    display( LOG_ERR, "Unable to get current time" );

    /* cleanup buffers */
    cleanup();
    return EXIT_FAILURE;
  }

  /* initialize program wide config options */
  config->hostname = (char *)XMALLOC( MAXHOSTNAMELEN+1 );

  /* get processor hostname */
  if ( gethostname( config->hostname, MAXHOSTNAMELEN ) != 0 ) {
    display( LOG_ERR, "Unable to get hostname" );
    strcpy( config->hostname, "unknown" );
  }

  config->cur_pid = getpid();

  /* setup current time updater */
  signal( SIGALRM, ctime_prog );
  alarm( 60 );

  /*
   * get to work
   */

  initParser();

  while (optind < argc) {
    printf( "Opening [%s] for read\n", argv[optind] );
#ifdef HAVE_FOPEN64
    if ( ( inFile = fopen64( argv[optind], "r" ) ) EQ NULL ) {
#else
    if ( ( inFile = fopen( argv[optind], "r" ) ) EQ NULL ) {
#endif
      fprintf( stderr, "ERR - Unable to open file [%s] %d (%s)\n", argv[1], errno, strerror( errno ) );
      return( EXIT_FAILURE );
    }

    optind++;

    while( fgets( inBuf, sizeof( inBuf ), inFile ) != NULL & ! quit ) {
      if ( reload EQ TRUE ) {
	fprintf( stderr, "Processed %d lines/min\n", lineCount );
	lineCount = 0;
	reload = FALSE;
      }
      if ( config->debug >= 3 )
	printf( "DEBUG - Before [%s]", inBuf );
      if ( ( ret = parseLine( inBuf ) ) > 0 ) {

	for ( i = 0; i < ret; i++ ) {
	  getParsedField( oBuf, sizeof( oBuf ), i );
	  if ( config->debug >= 4 )
	    printf( "DEBUG - [%s]\n", oBuf );
	  else if ( config->debug >= 1 )
	    if ( i EQ 0 )
	      printf( "DEBUG - [%s]\n", oBuf );
	}
	lineCount++;
      }
    }

    fclose( inFile );
  }
    
  deInitParser();

  /*
   * finished with the work
   */

  cleanup();

  return( EXIT_SUCCESS );
}

/****
 *
 * display prog info
 *
 ****/

void show_info( void ) {
  fprintf( stderr, "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
  fprintf( stderr, "By: Ron Dilley\n" );
  fprintf( stderr, "\n" );
  fprintf( stderr, "%s comes with ABSOLUTELY NO WARRANTY.\n", PROGNAME );
  fprintf( stderr, "This is free software, and you are welcome\n" );
  fprintf( stderr, "to redistribute it under certain conditions;\n" );
  fprintf( stderr, "See the GNU General Public License for details.\n" );
  fprintf( stderr, "\n" );
}

/*****
 *
 * display version info
 *
 *****/

PRIVATE void print_version( void ) {
  printf( "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
}

/*****
 *
 * print help info
 *
 *****/

PRIVATE void print_help( void ) {
  print_version();

  fprintf( stderr, "\n" );
  fprintf( stderr, "syntax: %s [options]\n", PACKAGE );

#ifdef HAVE_GETOPT_LONG
  fprintf( stderr, " -d|--debug (0-9)     enable debugging info\n" );
  fprintf( stderr, " -h|--help            this info\n" );
  fprintf( stderr, " -v|--version         display version information\n" );
#else
  fprintf( stderr, " -d {lvl}   enable debugging info\n" );
  fprintf( stderr, " -h         this info\n" );
  fprintf( stderr, " -v         display version information\n" );
#endif

  fprintf( stderr, "\n" );
}

/****
 *
 * cleanup
 *
 ****/

PRIVATE void cleanup( void ) {
  XFREE( config->hostname );
#ifdef MEM_DEBUG
  XFREE_ALL();
#else
  XFREE( config );
#endif
}

/*****
 *
 * interrupt handler (current time)
 *
 *****/

void ctime_prog( int signo ) {
  time_t ret;

  /* disable SIGALRM */
  signal( SIGALRM, SIG_IGN );
  /* update current time */
  reload = TRUE;

  /* reset SIGALRM */
  signal( SIGALRM, ctime_prog );
  /* reset alarm */
  alarm( 60 );
}
