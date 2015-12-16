/*
   GLOST - Greedy Launcher Of Small Tasks.
   Copyright (C) 2014  Bruno FROGE <bruno.froge@cea.fr>

   Author: Vincent DUCROT     <vincent.ducrot.tgcc@cea.fr>
   Author: Yohan LEE-TIN-YIEN <yohan.lee-tin-yien.tgcc@cea.fr>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

*/

/* Usage:
 *    glost_bcast FILE[...]
 *
 * Broadcast the file FILE into TMPDIR
 *
 *
 * Author: Yohan LEE-TIN-YIEN <yohan.lee-tin-yien.tgcc@cea.fr>
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <assert.h>


#ifndef  DEFAULT_BUFFLEN
#define DEFAULT_BUFFLEN 134217728
#endif

#define MPI_INT_ISTRUE  1
#define MPI_INT_ISFALSE 0

#define VERSION_MAJOR   0
#define VERSION_MINOR   3
#define VERSION_PATCH   1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* http://programanddesign.com/cpp/human-readable-file-size-in-c/ */
char* readable_fs(double size/*in bytes*/, char *buf) {
  int i = 0;
  const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  while (size > 1024) {
    size /= 1024;
    i++;
      }
  sprintf(buf, "%.*f %s", i, size, units[i]);
  return buf;
}

void usage(int argc,char **argv){
  printf("usage: [mpirun] [mpirun_options] %s FILE [...]\n\n"
	 "%s copies a shared FILE into each local directory $TMPDIR using MPI-IO.\n"
	 "Copies are done by creating the temporary files $TMPDIR/FILE.<rank> and a final renaming.\n\n"
	 "Version: \n  %i.%i.%i \n\n"
	 "Examples:\n"
	 "  mpirun -n 4 %s file \n"
	 "  ccc_mprun -n 4 -c 16 %s file \n"
	 ,basename(argv[0]),basename(argv[0])
	 ,VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH
	 ,argv[0],argv[0]
	 );
}


/* ******************************* */

int main(int argc,char **argv)
{
  int rank,size;
  double timing;

  /* init */
  MPI_Init(&argc,&argv);

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  /* read */
  const int filenamesz=4095;
  char *odirname, *ibname;
  char ifilename[filenamesz+1];
  char ofilename[filenamesz+1];
  char ofilename2[filenamesz+1];
  MPI_File ifh,ofh;
  MPI_Info ifinfo;
  char ctmp[100];
  char ctmp2[100];
  char ctmp3[100];

  size_t  bufflen = DEFAULT_BUFFLEN ;
  int iamode,oamode;

  long long fsize, bwritten, bread ;
  long long count;

  char iinfokey[MPI_MAX_INFO_KEY];
  int  iflag;
  int  ierr;

  /* early exit */
  if ((argc == 1) && (rank == 0))
      usage(argc,argv);

  sprintf(ofilename2,"%s","unset");

  for (int arg=1; arg<argc; arg++){
    char *buff1,*buff2;
    /*
     * modes
     */

    iamode = (int) 0 ;
    iamode |= MPI_MODE_RDONLY ;
    iamode |= MPI_MODE_UNIQUE_OPEN ;

    oamode = (int) 0 ;
    oamode |= MPI_MODE_CREATE ;
    oamode |= MPI_MODE_WRONLY ;
    oamode |= MPI_MODE_UNIQUE_OPEN ;

    /*
     * Infos
     */

    MPI_Info_create(&ifinfo);
    MPI_Info_set(ifinfo,"access_style","read_once");
    MPI_Info_set(ifinfo,"collective_buffering","true");
    /* MPI_Info_set(ifinfo,"cb_block_size","1048576"); */ /* 1 MBytes - should match FS block size */
    MPI_Info_set(ifinfo,"cb_block_size","4194304"); /* 4 MBytes - should match FS block size */
    MPI_Info_set(ifinfo,"cb_buffer_size","134217728"); /* 128 MBytes (Optional) */

    /*
     *  filenames
     */

    /* sprintf(ifilename,"%s","./testin.bin"); */
    snprintf(ifilename,filenamesz,"%s",argv[arg]);
    ibname = basename(ifilename);
    /* odirname = secure_getenv("TMPDIR"); */
    odirname = getenv("TMPDIR");
    if (odirname == NULL){
      fprintf(stderr,"#Error: TMPDIR environment variable must be set");
      MPI_Abort(MPI_COMM_WORLD,1);
    }
    odirname = strdup(odirname);
    snprintf(ofilename,filenamesz,"%s/%s.%i",odirname,ibname,rank);
    snprintf(ofilename2,filenamesz,"%s/%s",odirname,ibname);
    free(odirname);
    timing = MPI_Wtime();

    /*
     * Open
     */

    ierr = MPI_File_open(MPI_COMM_WORLD,ifilename, iamode, ifinfo, &ifh);
    if( ierr != MPI_SUCCESS ){
      if (rank == 0 )
	fprintf(stderr,"#Warning: cannot open %s, ignoring it\n", ifilename );
      continue;
    }

#ifndef DO_NOT_WRITE
    ierr = MPI_File_open(MPI_COMM_SELF,ofilename, oamode, MPI_INFO_NULL,&ofh);
    assert( ierr == MPI_SUCCESS );
#endif

    /* match the cb_nodes and striping */
    MPI_File_get_info(ifh,&ifinfo);
    MPI_Info_get(ifinfo,"striping_factor",MPI_MAX_INFO_KEY,iinfokey,&iflag);
    if( iflag == MPI_INT_ISTRUE ){
      if ( rank == 0)
	fprintf(stderr,"#stripping detected, setting cb_nodes : %s .\n", iinfokey );
      MPI_Info_set(ifinfo,"cb_nodes",iinfokey);

      /* reopen to be sure */
      ierr = MPI_File_close(&ifh);
      assert( ierr == MPI_SUCCESS );

      ierr = MPI_File_open(MPI_COMM_WORLD,ifilename, iamode, ifinfo, &ifh);
      assert( ierr == MPI_SUCCESS );
    }

    /*
     * Transfert
     */

    MPI_File_get_size(ifh, &fsize);
#ifndef DO_NOT_WRITE
    MPI_File_preallocate(ofh, fsize);
#endif
    buff1 = malloc(bufflen);
    buff2 = malloc(bufflen);

    if (rank == 0)
      fprintf(stderr,"Each process uses 2 buffers of %s \n",
	      readable_fs( (double) bufflen, ctmp3) );

    /* Assume read bandwidth >> write bandwidth
     * Thus non-blocking read minimise waiting time
     */

    bwritten = 0;
    bread    = 0;
    count    = 0;
    size_t  bufflen1 = bufflen ;
    size_t  bufflen2 = bufflen ;

    /* Handle first read */
    bufflen1 = MIN(fsize-bwritten,bufflen1) ;
    ierr = MPI_File_read_at_all_begin(ifh, bread, buff1, bufflen1, MPI_BYTE);
    assert( ierr == MPI_SUCCESS );
    /*  */
    while( bwritten < fsize){
      if (count%2 == 0 ){
	ierr = MPI_File_read_at_all_end(ifh,buff1,MPI_STATUS_IGNORE);
	assert( ierr == MPI_SUCCESS );
	bufflen1 = MIN(fsize-bwritten,bufflen1) ;
	bread    += bufflen1 ;

	bufflen2 = MIN(fsize-bread   ,bufflen2);
	if ( bufflen2 > 0 )
	  ierr = MPI_File_read_at_all_begin(ifh, bread, buff2, bufflen2, MPI_BYTE);
	assert( ierr == MPI_SUCCESS );

#ifndef DO_NOT_WRITE
	ierr = MPI_File_write_at(ofh, bwritten, buff1, bufflen1, MPI_BYTE, MPI_STATUS_IGNORE);
	assert( ierr == MPI_SUCCESS );
#endif
	bwritten += bufflen1 ;

      } else {
	ierr = MPI_File_read_at_all_end(ifh,buff2,MPI_STATUS_IGNORE);
	assert( ierr == MPI_SUCCESS );
	bufflen2 = MIN(fsize-bwritten,bufflen2) ;
	bread    += bufflen2 ;

	bufflen1 = MIN(fsize-bread   ,bufflen1) ;
	if ( bufflen1 > 0 )
	  ierr = MPI_File_read_at_all_begin(ifh, bread, buff1, bufflen1, MPI_BYTE);
	assert( ierr == MPI_SUCCESS );

#ifndef DO_NOT_WRITE
	ierr = MPI_File_write_at(ofh, bwritten, buff2, bufflen2, MPI_BYTE, MPI_STATUS_IGNORE);
	assert( ierr == MPI_SUCCESS );
#endif
	bwritten += bufflen2 ;

      }
      count++;
    }

    /*
     * End
     */

#ifndef DO_NOT_WRITE
    ierr = MPI_File_close(&ofh);
    assert( ierr == MPI_SUCCESS );
#endif
    ierr = MPI_File_close(&ifh);
    assert( ierr == MPI_SUCCESS );
    ierr = MPI_Info_free(&ifinfo);
    assert( ierr == MPI_SUCCESS );

    timing = MPI_Wtime() - timing;

    ierr = rename(ofilename, ofilename2);
    if (ierr == -1)
      fprintf(stderr,"#Error: could not rename file %s (rank=%i) \n", ofilename, rank );

    fprintf(stderr,"#copy size, timing: %s %g (s) %s/s (rank=%i).\n",
	    readable_fs( (double)fsize , ctmp),
	    timing,
	    readable_fs( (double)(fsize/timing),ctmp2),
	    rank);

    /* free memory */
    free(buff1);
    free(buff2);

  }

  /* exit */

  MPI_Finalize();
}
