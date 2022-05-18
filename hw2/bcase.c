/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * handles signals SIGUSR1 and SIGUSR2 and acts as the base case for deal or no deal
 * 
 */
#define _POSIX_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

bool opened = false;
unsigned int caseNum;
unsigned int amount;

/**
 * sigusr1_handler - handles SIGUSR1 signals
 *
 * @sig_num: the signal identification number
 * Return: void
 */
static void sigusr1_handler(int sig_num) {
  char op = 1; //send one byte with a 1
  char cl = 0; //send one byte with a 0
  if(opened) {
    //if the case is opened then write a 1 otherwise write a 0 to stderr
    //BRETT SMITH helped me with this line (had trouble writing a single byte)
    if(-1 == write(STDERR_FILENO, &op, (size_t) sizeof(unsigned char))) {
      fprintf(stderr, "Failed to print to std error.\n");
      exit(-1);
    }
  }
    
  else {
    if(-1==write(STDERR_FILENO, &cl, (size_t) sizeof(unsigned char))) {
      fprintf(stderr, "Failed to print to std error.\n");
      exit(-1);
    }
  }
}

/**
 * sigusr2_handler - handles SIGUSR2 signals
 *
 * @sig_num: the signal identification number
 * Return: void
 */
static void sigusr2_handler(int sig_num) {
  
  if(!opened) {
    opened = true;
    if(-1 == write(STDERR_FILENO, &amount, (size_t) sizeof(unsigned int))) {
      fprintf(stderr, "Failed to print to std error.\n");
      exit(-1);
    }
  }
  else {
    unsigned int y = 0;
    if(-1 == write(STDERR_FILENO, &y, (size_t) sizeof(unsigned int))) {
      fprintf(stderr, "Failed to print to std error.\n");
      exit(-1);
    }
  }
}

int main(int argc, char * argv[]) {
  
  //ensures that there are the correct num of arguments
  if(argc != 3){
    printf("Exactly two command line arguments required you had %d\n", argc-1);
    exit(-1);
  }
  opened = false;

  //converts commandline arguments to unsigned ints
  caseNum = atoi(argv[1]);
  amount  = atoi(argv[2]);

  pid_t pid = getpid();

  printf("Case number %d is PID %d\n", caseNum, (int)pid);

  sigset_t mask;
  sigemptyset(&mask);
  struct sigaction sa1 = {
    .sa_handler = sigusr1_handler,
    .sa_mask = mask,
    .sa_flags = 0
  };
  struct sigaction sa2 = {
    .sa_handler = sigusr2_handler,
    .sa_mask = mask,
    .sa_flags = 0
  };
  
  //sets signal handlers
  sigaction(SIGUSR1, &sa1, NULL);
  sigaction(SIGUSR2, &sa2, NULL);

  size_t x;
  char c;
  //infinite loop that waits for control-D from console.I Helped Brett Smith with this.
  do {
    x = read(STDIN_FILENO, &c, 1);
  }while(x != 0);

  fprintf(stdout, "Control-D Pressed, ending process %d\n", (int)pid); 
}
