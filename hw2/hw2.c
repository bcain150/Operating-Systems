/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * the main program for the deal or no deal game.
 * in general i worked with Tadewos Bellete on a lot of this
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

#define _GNU_SOURCE
#define NUM_CASES 8

struct pipeData {
  int fdP1[2], fdP2[2]; // index 0 is read and index 1 is write
  pid_t PID;
};

int main(int argc, char *argv[]) {
  //if there is any more than 1 command line argument then quit
  if(argc != 2) {
    fprintf(stdout, "One and only one command line argument needed.\n");
    exit(-1);
  }

  unsigned int seed = atoi(argv[1]);
  
  unsigned case_amounts[NUM_CASES];
  srand(seed);
  for (int i = 0; i < sizeof(case_amounts) / sizeof(case_amounts[0]); i++) {
    case_amounts[i] = rand() % 1000000 + 1;
  }

  //creates an array of struct pipeData
  struct pipeData pipes[NUM_CASES];

  //creates write and read pipes for each process in pipeData
  for(int i=0; i<NUM_CASES; i++) {
    if(-1 == pipe(pipes[i].fdP1)) {
      fprintf(stdout, "Failed to create read end of pipe %d\n", i);
      exit(-1);
    }
    if(-1 == pipe(pipes[i].fdP2)) {
      fprintf(stdout, "Failed to create write end of pipe %d\n", i);
      exit(-1);
    }
  }

  bool isParent = true;
  //creates child process for each set of pipes
  for(int i=0; i<NUM_CASES; i++) {
    pipes[i].PID = fork();
    if(pipes[i].PID == -1) {
      fprintf(stdout, "Forking with pipeData %d failed\n", i);
      exit(-1);
    }
    else if(pipes[i].PID == 0){// is a child process
      //dup certain pipes and close others??

      if(-1 == dup2(pipes[i].fdP1[0], STDIN_FILENO)) {
	fprintf(stdout, "could not dup Pipe 1 read.\n");
	exit(-1);
      }
      if(-1 == close(pipes[i].fdP1[0])) {
	fprintf(stdout, "could not close pipe 1 read\n");
	exit(-1);  
      }
      if(-1 == dup2(pipes[i].fdP2[1], STDERR_FILENO)) {
	fprintf(stdout, "could not dup pipe 2 write.\n");
	exit(-1);
      }
      if(-1 == close(pipes[i].fdP2[1])) {
	fprintf(stdout, "could not close pipe 2 write.\n");
	exit(-1);
      }
      if(-1 == close(pipes[i].fdP1[1])) {
	fprintf(stdout, "could not close pipe 1 write.\n");
	exit(-1);
      }
      if(-1 == close(pipes[i].fdP2[0])) {
	fprintf(stdout, "could not close pipe 2 read.\n");
	exit(-1);
      }

      char caseNum[100];
      char amount[100];
      sprintf(caseNum, "%d", i);
      sprintf(amount, "%d", case_amounts[i]);
      
      //start an instance of bcase
      if(-1 == execlp("./bcase","bcase", caseNum, amount, (char*) NULL)) printf("execlp() failed\n");
      printf("IM STILL HERE\n");
      isParent = false;
      break;
    }
  }

  //FOR PARENT PROCESS: closes reading end of P1 pipes
  //and writing end of P2 Pipes
  if(isParent) {
    for(int i=0; i<NUM_CASES; i++) {
      //closes read on p1
      if(-1 == close(pipes[i].fdP1[0])) {
	fprintf(stdout, "Couldn't close p1 read on parent process\n");
	exit(-1);
      }
      //closes write on p2
      if(-1 == close(pipes[i].fdP2[1])) {
	fprintf(stdout, "Couldn't close p2 write on parent process\n");
	exit(-1);
      }
    }
    char *menu = "Main Menu:\n\t0. Quit Game\n\t1. Game Status\n\t2. Open Briefcase\n\0";
    char c[10];
    int choice = 1;
    int status;

    while(choice != -1) {
      printf("%s", menu);
      printf("Please pick an entry from the menu: ");
      if(-1 == scanf("%s", c)) exit(-1);
      choice = atoi(c);
      switch(choice) {
	
      case 0:
	
	for(int i=0; i<NUM_CASES; i++) {
	  if(-1 == close(pipes[i].fdP1[1])) exit(-1);
	  waitpid(pipes[i].PID, &status, 1);
	}
	choice = -1;
	break;
	
      case 1:
	
	break;
	
      case 2:
	break;
      default:
	break;
      }
    }
  }
  return 0;
}
