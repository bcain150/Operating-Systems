
#define _XOPEN_SOURCE 500
#define NUM_HOUSES 8

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

struct house {
  unsigned int X;
  unsigned int Y;
  unsigned int candy;
  unsigned int house_num;
  pthread_mutex_t lock;
};

struct group {
  unsigned int home;
  unsigned int currHouse;
  unsigned int nextHouse;
  unsigned int dist;
  unsigned int num_kids;
  unsigned int candy_cnt;
  pthread_t tid;
};

//GLOBALS
unsigned int sim_time;
unsigned int num_groups;
struct house hood_arr[NUM_HOUSES];
struct group * group_arr;
bool sim_done = false;
pthread_mutex_t sim_lock;

//Thread functions

static void *childGroups(void *args) {
  int groupNum = *(int *)(args);
  int next_house;
  int this_house;
  srand(time(NULL));
  
  pthread_mutex_lock(&sim_lock);
  while(!sim_done) {
    pthread_mutex_unlock(&sim_lock);
    
    //finds next house
    this_house = group_arr[groupNum].currHouse;
    do {
      next_house = rand()%NUM_HOUSES;
    }while(next_house != this_house);
    
    //compute manhattan distance
    int taxiDist = abs(hood_arr[next_house].X - hood_arr[this_house].X) +
      abs(hood_arr[next_house].Y - hood_arr[this_house].Y);
    group_arr[groupNum].dist = taxiDist;

    printf("Group %d: from house %d to %d (travel time = %d ms)\n", groupNum, this_house, next_house, 250*taxiDist);
   
    //sleep for travel time
    usleep(250000*taxiDist);
    group_arr[groupNum].currHouse = next_house;
    this_house = next_house;
    
    //locks house to grab candy
    if(this_house != group_arr[groupNum].home) {
      pthread_mutex_lock(&hood_arr[this_house].lock);
      if(group_arr[groupNum].num_kids > hood_arr[this_house].candy) {
	group_arr[groupNum].candy_cnt += hood_arr[this_house].candy;
	hood_arr[this_house].candy = 0;
      }
      else {
	group_arr[groupNum].candy_cnt += group_arr[groupNum].num_kids;
	hood_arr[this_house].candy -= group_arr[groupNum].num_kids;
      }
      pthread_mutex_unlock(&hood_arr[this_house].lock);
    }
    pthread_mutex_lock(&sim_lock);
  }
  pthread_mutex_unlock(&sim_lock);
  return NULL;
}

static void *neighborhood(void *args) {
  FILE *fp = args;
  int size = 10;
  char buff[size];
  
  int house_num;
  int candy_amt;
  
  while(fgets(buff, size, fp) != NULL) {
    pthread_mutex_lock(&sim_lock);
    if(sim_done) {
      pthread_mutex_unlock(&sim_lock);
      break;
    }
    pthread_mutex_unlock(&sim_lock);
    
    usleep(250000);
    char *str = strtok(buff, " ");
    house_num = atoi(str);
    candy_amt = atoi(strtok(NULL, " "));

    printf("Neighborhood: added %d to %d\n", candy_amt, house_num);
    pthread_mutex_lock(&hood_arr[house_num].lock);
    hood_arr[house_num].candy += candy_amt;
    pthread_mutex_unlock(&hood_arr[house_num].lock);
  }
  return NULL;
}

int main(int argv, char * argc[]) {

  //Checks command line args
  if(argv != 2) {
    fprintf(stderr, "Please enter a single file name as a commandline arg.\n");
    exit(-1);
  }

  //opens file
  char * file = argc[1];
  FILE * fp = fopen(file, "r" );
  if(fp == NULL) {
    fprintf(stderr, "Could not open file.\n");
    exit(-1);
  }

  //initializes buffer and reads sim_time and num_groups
  int size = 10;
  char buff[size];
  if(fgets(buff, size, fp) == NULL) {
    fprintf(stderr, "Could not read file.\n");
    exit(-1);
  }
  sim_time = atoi(buff);
  //gets num_groups
  if(fgets(buff, size, fp) == NULL) {
    fprintf(stderr, "Could not read file.\n");
    exit(-1);
  }
  num_groups = atoi(buff);

  //parses remaining file up to groups
  for(int i=0; i<NUM_HOUSES; i++) {
    if(fgets(buff, size, fp) == NULL) {
      fprintf(stderr, "Could not read line %d of file.\n", i+3);
      exit(-1);
    }

    //string is delimited by spaces get each int
    char * str = strtok(buff, " ");
    hood_arr[i].X = atoi(str);
    hood_arr[i].Y = atoi(strtok(NULL, " "));
    hood_arr[i].candy = atoi(strtok(NULL, " "));
    hood_arr[i].house_num = i;
    pthread_mutex_init(&hood_arr[i].lock, NULL);
    //all house data is now in hood_arr data struct
  }

  //parses group data and initializes the groups data structure
  struct group arr[num_groups];

  for(int i=0; i<num_groups; i++) {
    if(fgets(buff, size, fp) == NULL) {
      fprintf(stderr, "Could not read line %d of file.\n", i+11);
      exit(-1);
    }

    char *str = strtok(buff, " ");
    arr[i].currHouse = atoi(str);
    arr[i].home = atoi(str);
    arr[i].num_kids = atoi(strtok(NULL, " "));
    arr[i].candy_cnt = 0;
  }

  //sets the array equivalent to a global array
  group_arr = arr;

  pthread_mutex_init(&sim_lock, NULL);
  
  //creates threads for each group
  int j, data[num_groups];
  for(j=0; j<num_groups; j++) {
    data[j] = j;
    if(pthread_create(&group_arr[j].tid, NULL, childGroups, data + j) != 0){
      fprintf(stderr, "Could not create trick-or-treaters thread number %d.\n", j);
      exit(-1);
    }
  }
  
  //create neighborhood thread
  pthread_t hood;
  if(pthread_create(&hood, NULL, neighborhood, fp) != 0){
    fprintf(stderr, "Could not create neighborhood thread.\n");
    exit(-1);
  }

  int seconds = 0;
  int end;
  int kids;
  int numCandy;
  int totalCandy = 0;
  int x, y;
  while(seconds < sim_time) {
    //print data
    printf("After %d seconds:\n", seconds);
    printf("\tGroup statuses:\n");
    for(int i=0; i<num_groups; i++) {
      kids = group_arr[i].num_kids;
      end = group_arr[i].nextHouse;
      numCandy = group_arr[i].candy_cnt;
      totalCandy += numCandy;
      printf("\t\t%d\tsize %d, going to %d, collected %d\n", i, kids, end, numCandy);
    }
    printf("\tHouse statuses:\n");
    for(int i=0; i<NUM_HOUSES; i++) {
      x = hood_arr[i].X;
      y = hood_arr[i].Y;
      numCandy = hood_arr[i].candy;
      printf("\t\t%d @ (%d, %d): %d available\n", i, x, y, numCandy);
    }
    printf("Total Candy: %d\n", totalCandy);
    sleep(1);
    seconds++;
  }
  pthread_mutex_lock(&sim_lock);
  sim_done = true;
  pthread_mutex_unlock(&sim_lock);
  
  fclose(fp);

  //join neighborhood
  pthread_join(hood, NULL);
  //join groups
  for(int i=0; i<num_groups; i++) {
    pthread_join(group_arr[i].tid, NULL);
  }
  
  return 0;
}

