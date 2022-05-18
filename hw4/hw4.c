#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define NUM_SEATS 6
#define NUM_TABLES 16

extern void hw4_test(void);

struct party{ //contains the number of tables and number of guests
  //only populated if lead party table otherwise 0
  size_t num_tables;
  size_t guests;
  //stores the index of lead party table
  //-1 if its an open table
  int p_index;
};

char restaurant[NUM_TABLES][NUM_SEATS]; //stores all guests
struct party parties[NUM_TABLES]; //stores the party at the index of first table
unsigned int waiting_parties;
unsigned int availSeats;
pthread_cond_t party_ready;
pthread_mutex_t party_ready_m;

/**
 * Reserve table(s) for the requested number of guests.
 *
 * Determine the minimal number of adjacent tables
 * necessary to seat @a num_guests guests. Then reserve the tables for
 * the guests. For each character in @a names, assign that guest to a
 * seat at the table(s).
 *
 * If the number of requested guests exceeds the restaurant's capacity,
 * then set errno to @c ENOMEM and return NULL.
 *
 * If insufficient tables are free to seat all of the guests, then
 * block the calling thread until enough tables are
 * freed.
 *
 * @param num_guests Number of guests to be seated. If @c 0, your code
 * may do whatever it wants. In the C standard, malloc() of @c 0 is "implementation
 * defined", meaning it is up to you if you want to return @c
 * NULL, segfault, whatever.
 * @param names Single-character names for each of the guests to be
 * seated.
 *
 * @return Opaque pointer for dining party, or @c NULL on error.
 */
void *reserve_table(size_t num_guests, const char *names) {

  
  //available seat overflow check
  //not enough seats available or no guests in reservation request
  if(num_guests > availSeats || num_guests == 0) {
    errno = ENOMEM;
    return NULL;
  }
  
  //lock data
  pthread_mutex_lock(&party_ready_m);
  
  //calculate the required number of seats
  int amtTables = 0;
  if(num_guests%NUM_SEATS != 0) amtTables++;
  amtTables += num_guests/NUM_SEATS;

  while(1) {
    for(int i=0; i<NUM_TABLES; i++) {//look through all tables for empty spot
      if(parties[i].p_index == -1) {//-1 p_index means no party
	int j;
	for(j=1; j<amtTables; j++) {//if there is a table in the way
	  if(parties[(i+j)%NUM_TABLES].p_index != -1) break;
	}
	if(j >= amtTables) { //if the j loop never broke

	  //adapt parties array
	  parties[i].p_index = i;
	  parties[i].num_tables = amtTables;
	  parties[i].guests = num_guests;
	  for(int x=1; x<amtTables; x++) {
	    parties[(i+x)%NUM_TABLES].p_index = i;
	    parties[(i+x)%NUM_TABLES].num_tables = 0;
	    parties[(i+x)%NUM_TABLES].guests = 0;
	  }

	  //change restaurant array
	  int t=0;
	  //for each character in names, reserve their seat
	  for(int x=0; x<num_guests; x++) {
	    if(x%NUM_SEATS == 0 && x != 0) t++;//if its a multiple of NUM_SEATS and not 0
	    restaurant[(i+t)%NUM_TABLES][x%NUM_SEATS] = names[x]; //set the character
	  }
	  availSeats -= num_guests;
	  pthread_mutex_unlock(&party_ready_m);
	  return &restaurant[i]; //return the entire table
	}
      }
    }
    
    waiting_parties++;
    pthread_mutex_unlock(&party_ready_m);
    pthread_cond_wait(&party_ready, &party_ready_m);
  }
}

/**
 * Return single-character name for a guest.
 *
 * Given the opaque pointer to a dining party, and a seat number,
 * return the character associated with that guest. The first seat at
 * the first table is seat number 0.
 *
 * If the opaque pointer is invalid, or no guest is sitting at the
 * requested seat, then set errno to @c EINVAL and return 0 (value 0,
 * not ASCII '0').
 *
 * Caution: If the original dinner party reserved multiple
 * seats, @a seat_num could be greater than 5.
 *
 * @return Guest name, or 0 on error.
 */
const char get_guest_name(void *party, size_t seat_num) {
  if(pthread_mutex_lock(&party_ready_m) != 0) return -1;
  //checks to see if valid table pointer
  bool isValid = false;
  int i;
  for(i=0; i<NUM_TABLES; i++) {
    if(&restaurant[i] == party) {
      isValid = true;
      break;
    }
  }
  if(isValid) {
    //gets char by calculating table and seat offsets
    int t = seat_num / NUM_SEATS;
    int s = seat_num % NUM_SEATS;
    //return the character
    if(pthread_mutex_unlock(&party_ready_m) != 0) return -1;
    return restaurant[(i+t)%NUM_TABLES][s];
  }
  else { // invalid so errored.
    errno = EINVAL;
    if(pthread_mutex_unlock(&party_ready_m) != 0) return -1;
    return 0;
  }
}

/**
 * Release the table(s) associated with a dining party. Guests that
 * were seated leave (i.e., clear the contents of seats at the
 * table(s), by setting the seats to value 0).
 *
 * If @a ptr is not a pointer returned by reserve_table(), then send a
 * @c SIGSEGV signal to the calling thread. Likewise, calling
 * release_table() on a previously released opaque pointer results in
 * a @c SIGSEGV.
 *
 * @param ptr Opaque pointer for a dining party. If @c NULL, do
 * nothing. Releasing a @c NULL pointer is a defined as a no-op.
 */
void release_table(void *party) {
  if(pthread_mutex_lock(&party_ready_m) != 0) exit(-1);
  //checks to see if valid table pointer
  bool isValid = false;
  int i;
  for(i=0; i<NUM_TABLES; i++) {
    if(&restaurant[i] == party) {
      isValid = true;
      break;
    }
  }
  
  if(parties[i].p_index == -1) isValid = false;
  
  if(isValid) {
    availSeats += parties[i].guests;

    //initialize offsets and clear party
    int t = 0;
    for(int j=0; j<parties[i].guests; j++) {
      if(j%NUM_SEATS == 0 && j != 0) t++;
      restaurant[(i+t)%NUM_TABLES][j%NUM_SEATS] = 0;
    }

    //clean up waiting party array;
    int tables = parties[i].num_tables;
    for(int j=0; j<tables; j++) {
      parties[(i+j)%NUM_TABLES].p_index = -1;
      parties[(i+j)%NUM_TABLES].num_tables = 0;
      parties[(i+j)%NUM_TABLES].guests = 0;
    }

    
    waiting_parties--;
    if(pthread_mutex_unlock(&party_ready_m) != 0) exit(-1);
    if(pthread_cond_signal(&party_ready) != 0) exit(-1);
    
  }
  else raise(SIGSEGV);
}

/**
 * Return the maximum dining party size that can be seated in the
 * restaurant at this time.
 *
 * Remember that a dining party must sit at adjacent tables, if the
 * party's size is greater than 6. If multiple tables are not
 * adjacent, then a dining party may not be split across those
 * discontiguous tables.
 *
 * Caution: Tables 15 and 0 are adjacent.
 *
 * @return Maximum party size
 */
size_t get_max_party_size(void) {
  if(pthread_mutex_lock(&party_ready_m) != 0) exit(-1);

  size_t max = 1;
  size_t temp;
  for(int i=0; i<NUM_TABLES; i++) {
    temp = parties[parties[i].p_index].guests;
    if(max < temp) max = temp;
  }  
  if(pthread_mutex_unlock(&party_ready_m) != 0) exit(-1);

  return max;
}

/**
 * Return the number of dining parties that are still waiting to be
 * seated at this time.
 *
 * @return Number of parties waiting
 */
unsigned int get_num_waiting(void) {
  return waiting_parties;
}

/**
 * Write to standard output information about the current state of the
 * restaurant.
 *
 * Display all tables. For each table, show the occupant of each seat,
 * or a dot if that seat is empty.
 *
 * Then show how many dining parties are still waiting to be seated.
 */
void print_restaurant_status() {
  if(pthread_mutex_lock(&party_ready_m) != 0) exit(-1);
  for(int i=0; i<NUM_TABLES; i++) {
    printf("%d: ", i);
    for(int j=0; j<NUM_SEATS; j++) {
      if(restaurant[i][j] != 0)
	printf("%d", restaurant[i][j]);
      else
	printf(".");
    }
    printf("\n");
  }
  printf("\tNumber of waiting parties: %d\n", waiting_parties);
  if(pthread_mutex_unlock(&party_ready_m) != 0) exit(-1);
}


int main() {

  if(pthread_cond_init(&party_ready, NULL) !=0) return -1;
  if(pthread_mutex_init(&party_ready_m, NULL) !=0) return -1;
  
  //initialize globals
  waiting_parties = 0;
  availSeats = 0;
  for(int i=0; i<NUM_TABLES; i++) {
    parties[i].p_index = -1;
  }
  for(int i=0; i<NUM_TABLES; i++) {
    for(int j=0; j<NUM_SEATS; j++) {
      availSeats++;
      restaurant[i][j] = 0;
    }
  }

  hw4_test();
  
  if(pthread_cond_destroy(&party_ready) != 0) return -1;
  if(pthread_mutex_destroy(&party_ready_m) != 0) return -1;
}
