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


#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>


#define GLOST_VERSION_MAJOR 0
#define GLOST_VERSION_MINOR 3
#define GLOST_VERSION_PATCH 1
static const char flavor[]="-tgcc";

#define GLOST_STRLEN 4096
#define GLOST_MASTER 0
#define TAG_AVAIL 1
#define TAG_TSK   2

/* core functions */
void read_and_exec(char *filename, double user_tremain, size_t len);
void read_and_send(int slaves, char *filename, double user_tremain, size_t len);
void recv_and_exec(int rank, size_t len);
static void read_next_command(FILE *fl,char *tsk, double user_tremain, size_t len);
static void exec_command(char *tsk, int rank);

/* options handling */
int read_options(int argc, char *argv[],double *user_tremain,size_t *maxstrlen);
void print_options(char *argv0);

/* signal handling */
void set_sigaction();
static void set_glost_no_new_task(int signum);

#ifdef __HAVE_LIBCCC_USER__
#include "ccc_user.h"
#else
static int ccc_tremain(double *tremain)
{
  tremain[0] = 0. ;
  return EXIT_FAILURE;
}
#endif

/********
 * main *
 ********/

static volatile int glost_no_new_task=0;


int main(int argc,char **argv)
{
  int rank,size;
  int tmp;
  double user_tremain = -1.0 ;
  size_t maxstrlen = GLOST_STRLEN ;

  /* init */
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);  

  tmp=0;
  set_sigaction();

  /* read options */
  if (rank == GLOST_MASTER)
    tmp=read_options(argc,argv,&user_tremain,&maxstrlen);
  
  MPI_Bcast(&tmp,1,MPI_INT,0,MPI_COMM_WORLD);
  if (tmp != 0){
    MPI_Finalize();
    exit(0);
  }
  MPI_Bcast(&maxstrlen,1,MPI_UINT64_T,0,MPI_COMM_WORLD);

  if (rank == GLOST_MASTER) {
    printf("master is %d , nb slaves: %d\n", GLOST_MASTER,size-1);	
  }
  
  /* launch */
  if (size == 1)
    read_and_exec(argv[optind],user_tremain,maxstrlen);
  else if(size != 1 && rank == GLOST_MASTER)
    read_and_send(size-1,argv[optind],user_tremain,maxstrlen);
  else
    recv_and_exec(rank,maxstrlen);

  /* exit */
  MPI_Finalize();
}

/****************** 
 * core functions *
 ******************/

void read_and_exec(char *filename, double user_tremain, size_t len )
{
  FILE *fl;
  char tsk[len];

  fl = fopen(filename,"r");
  if (fl == NULL){
    fprintf(stderr,"#WARNING could not open %s: skipping it.\n", filename);
    return;
  }
  do {
    read_next_command(fl,tsk,user_tremain,len);
    if(!strlen(tsk))
      break;
    exec_command(tsk,GLOST_MASTER);
  } while (strlen(tsk));
  fclose(fl);
}

void read_and_send(int slaves,char *filename, double user_tremain, size_t len)
{
  int i, num;
  MPI_Status status;
  FILE *fl;
  char tsk[len];

  fl = fopen(filename,"r");
  if (fl == NULL){
    fprintf(stderr,"#WARNING could not open %s: skipping it.\n", filename);
    /* the rest is handled in read_next_command() to properly kill the processes */
  }

  i=0;
  do {
    read_next_command(fl,tsk,user_tremain,len);
    
    MPI_Recv(&num,1,MPI_INT,MPI_ANY_SOURCE,
	     TAG_AVAIL,MPI_COMM_WORLD,&status);
    MPI_Send(tsk,len,MPI_CHAR,num,
	     TAG_TSK,MPI_COMM_WORLD);
    if (!strlen(tsk)) /* terminate command sent */
      i++;
  } while (i < slaves);
  fclose(fl);
}

void recv_and_exec(int rank,size_t len)
{
  char tsk[len];
  MPI_Status status;  

  do {
    MPI_Send(&rank,1,MPI_INT,GLOST_MASTER,
	     TAG_AVAIL,MPI_COMM_WORLD); 
    MPI_Recv(tsk,len,MPI_CHAR,GLOST_MASTER,
	     TAG_TSK,MPI_COMM_WORLD,&status); 
    if(!strlen(tsk)) /* terminate command */
      break;
    exec_command(tsk,rank);
  } while (strlen(tsk));
}

static void read_next_command(FILE *fl,char *tsk, double user_tremain, size_t len)
{
  int r;
  double tremain;

  tsk[0]='\0'; 

  r = ccc_tremain(&tremain); 	
  r = (!r && tremain < user_tremain);
  if (r) 
    fprintf(stderr,"#time remaining = %g < %g : do not launch new tasks.\n",
	    tremain,user_tremain);

  if (glost_no_new_task) {
      fprintf(stderr,"#signal received: no new task will be submitted\n");
      r = 1;
  }

  if (r) return;

  /* if(!fgets(tsk,GLOST_MAXLEN,fl)) tsk[0]='\0'; */
  /* if(tsk[strlen(tsk)-1]=='\n') tsk[strlen(tsk)-1]='\0'; */

  do {
    if(!fgets(tsk,len,fl)){
      tsk[0] = '\0';
      break;
    } 
    r = strlen(tsk);
  } while (r < 2);

  if(tsk[r-1]=='\n') tsk[r-1]='\0';

}

static void exec_command(char *tsk, int rank)
{
  double t;
  int r;

  t = MPI_Wtime();  
  r = system(tsk);
  t = MPI_Wtime() - t;
  if ( WIFEXITED(r) ) 
    {
      r = WEXITSTATUS(r); 
      fprintf(stderr,"#executed by process\t%d in %gs\twith status\t%d : %s\n",
	      rank,t,r,tsk);
    }
  else { 
    if( WIFSIGNALED(r) ){
      r = WTERMSIG(r); 
      fprintf(stderr,"#executed by process\t%d in %gs\tkilled by signal %d: %s\n",
	      rank,t,r,tsk);
    }
  }
}


/******************** 
 * option functions *
 ********************/


int read_options(int argc, char *argv[],double *user_tremain,size_t *maxstrlen)
{
  int r;
  int longoptind;
  user_tremain[0] = -1.;
  maxstrlen[0] = GLOST_STRLEN ;

  static struct option longopts[]=
    {
      {"help", no_argument, NULL, 'h' },
      {"version", no_argument, NULL, 'v' },
      {"time_remaining",required_argument,NULL,'R'},
      {"maxline_length",required_argument,NULL,'l'},
      {0,0,0,0}
    };

  while ( (r = getopt_long(argc, argv,"hvl:R:",longopts,&longoptind)) != -1 )
    {
      if (r == 0){ 		/* long option */
	fprintf(stderr,"r %c\n",longopts[longoptind].val);	
	r = longopts[longoptind].val ;
      }
	
      switch(r)
	{
	case 'h': 
	  print_options(argv[0]);
	  return 1;
	  break;
	case 'v': 
	  fprintf(stdout,"%d.%d.%d%s\n",GLOST_VERSION_MAJOR,GLOST_VERSION_MINOR,GLOST_VERSION_PATCH,flavor);	
	  return 1;
	  break;
	case 'l':
	  errno = 0;    /* To distinguish success/failure after call */
	  *maxstrlen = strtoul(optarg,NULL,10);
	  if (errno != 0) {
	    perror("strtol");
	    return 1;
	  }
	  break;
	case 'R': 
	  *user_tremain = strtod(optarg,NULL);
#ifndef   __HAVE_LIBCCC_USER__
	  fprintf(stderr,"WARNING option -R %g ignored: not compiled with libccc_user\n",*user_tremain);
#endif
	  break;

	case '?':
	  fprintf(stderr,"WARNING option %c ignored: unknown option\n",optopt);	
	  return 1;
	  break;

	default:
	  return 1;
	  break;
	}
    }

  if (optind == argc) {
    fprintf(stderr,"ERROR: no input file. Exiting...\n");
    return 1;
  }

  return 0;
}

void print_options(char *argv0){

  /* NAME */
  printf("\nNAME\n\n");
  printf("glost_launch - Greedy launcher Of Small Tasks.\n");

  /* SYNOPSIS */
  printf("\nSYNOPSIS\n\n");
  printf("Usage: [mpirun [mpi_option]...] %s [OPTION] TASK_FILE... \n", argv0);  

  printf("\n"
	 "Where \'mpirun\', is your MPI launcher,\n"
	 "'mpi_option\' is your MPI option.\n"
	 "'TASK_FILE\' is the file containing the independent tasks,\n"
	 "one task per line.\n"
	 );

  /* OPTION */
  printf("\nOPTION\n\n");

  printf(""
    "-h, --help          display this help and exit\n"
    "-v, --version       display version information and exit\n"
    "-l,--maxline_length[=]size \n"
    "   specify the maximum number of character per line in the input file \n"
    "    (default is %i , maximal value is %lu)\n"
    "-R,--time_remaining[=]time \n"
    "   stop submit new tasks if the time remaining for the job is inferior to \'time\' \n"
    "   It is only available if you have compiled it with libccc_user. \n"
    "   (-D __HAVE_LIBCCC_USER__) \n",GLOST_STRLEN,ULONG_MAX );

  /* DESCRIPTION */

  printf("\nDESCRIPTION\n\n");

  printf(""
  "\'%s\' try to execute simultaneously \"number of MPI processes -1\" lines\n"
  "of \'TASK_FILE\' in parrallel.\n"
  "\n"
  "This will help you launching a maximum of independent tasks,\n"
  "with variable execution time under the limitations of your cluster batch sheduler.\n",argv0);

  printf(""
  "\n"
  "  - Algorithm -\n"
  "\n"
  "One invocation of %s starts an MPI job.\n"
  "Process 0 is called master, while the others, slaves.\n"
  "\n"
  "The master reads the next task, waits for \n"
  "a free slave, and sends the task to the free slave. \n"
  "\n"
  "Meanwhile, each slave says that he is free to the master, \n"
  "waits for the task, and executes it. \n"
  "\n"
  "If %s is launched in sequential, the master  \n"
  "reads and executes himself each task.  \n"
  "\n"
  ,argv0,argv0);

  printf(
  "\n"
  "  - Outputs -\n"
  "\n"
  "Each task is executed with \'system(3)\', timed, and\n"
  "its status is logged on stderr, using the following format : \n"
  "\n"
  "  #executed by process <rank> in <time>s with status <status> : <task> \n"
  "\n");

  printf(
  "\n"
  "  - Ending %s during execution. -\n"
  "\n"
  "Instead of using the traditional SIGQUIT, use SIGUSR1 to kill \n"
  "%s properly (to make %s stop launching new commands).\n"
  "\n"
  "Unfortunately, we cannot use the traditional SIGQUIT \n"
  "because OpenMPI and its derivates filter the signal.  \n"
  "For an example, see example/test_sigusr1.sh  \n"
  ,argv0,argv0,argv0);

  printf(
  "\n"
  "  - Process Environment -\n"
  "\n"
  "The task launched inherits their environment from the user's shell.\n"
  "\n"
  "Thus user may use exported variables in the \'TASK_FILE\' file.\n"
  "We do advise using them for paths.\n"
  );

  printf(
  "\n"
  "  - Process Environment -\n"
  "\n"
  "If %s is killed, it should kill every slave.   \n"
  "\n"
  "If a task fails, its returned code is showed in <status> in the outputs.  \n"
  "The user may use glost_filter.sh to extracts from a logged stderr,  \n"
  "which task was run, which one are remainings, etc. \n",argv0);

  /* See also */
  printf("\nSEE ALSO\n\n");
  printf("glost_filter.sh -h\n");

}


/******************* 
 * signal handling *
 *******************/

void set_sigaction()
{
  struct sigaction new_action;

  new_action.sa_handler = set_glost_no_new_task ;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = SA_RESETHAND;

  if (sigaction(SIGUSR1,&new_action,NULL) < 0)
    fprintf(stderr,"#WARNING sigaction failed: no SIGQUIT support\n");
}

static void set_glost_no_new_task(int signum)
{
  glost_no_new_task=1;
}

