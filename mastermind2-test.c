/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * This file is the unit test program for mastermind.c
 */

#include "cs421net.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <stdlib.h>

int mm_fd;			/* file descritor for /dev/mm (read/write) */
int mm_ctl_fd;			/*file descriptor for /dev/mm_ctl (write only) */

/** start_mastermind
    helper function used
    to open device drivers and start the game
    returns 0 on success -1 on true
    
 */
int start_mastermind()
{
	mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}

	if (write(mm_ctl_fd, "start", 5) == -1)
		return -1;

	return 0;
}

/** quit_mastermind()

    helperfunction to end game and quit drivers
*/
int quit_mastermind()
{
	if (write(mm_ctl_fd, "quit", 4) == -1)
		return -1;
	if (close(mm_fd) == -1) {
		printf("Failed to close /dev/mm\n");
		return -1;
	}
	if (close(mm_ctl_fd) == -1) {
		printf("Failed to close /dev/mm_ctl\n");
		return -1;
	}

	return 0;
}

int mmap_test()
{

	if (start_mastermind() != 0) {
		printf("\nFailed to start mastermind\n");
		return -1;
	}

	if (write(mm_fd, "1234", 4) == -1)
		return -1;
	if (write(mm_fd, "4321", 4) == -1)
		return -1;
	char *c = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, mm_fd, 0);

	printf("%s\n", c);

	if (quit_mastermind() != 0) {
		printf("\nFailed to end mastermind\n");
		return -1;
	}

	return 0;
}

int happy_path_test()
{
	char buf[10];

	if (start_mastermind() != 0) {
		printf("\nFailed to start mastermind\n");
		return -1;
	}

	if (write(mm_fd, "1234", 4) == -1)
		return -1;

	if (read(mm_fd, buf, 4) == -1)
		return -1;

	printf("%s\n\n", buf);

	if (quit_mastermind() != 0) {
		printf("\nFailed to end mastermind\n");
		return -1;
	}

	return 0;
}

int illegal_control()
{
	if (start_mastermind() != 0) {
		printf("\nFailed to start mastermind\n");
		return -1;
	}

	if (write(mm_ctl_fd, "started", 7) == -1) {
		printf("Unable to write \"started\" to the device.\n");
		//successful
	} else {
		printf("write \"started\" to the device was successful\n");
		return -1;
	}
	if (write(mm_ctl_fd, "start\n", 6) == -1) {
		printf("Unable to write \"start\\n\" to the device.\n");
		//successful
	} else {
		printf("writing \"start\\n\" to the device was successful\n");
		return -1;
	}

	if (quit_mastermind() != 0) {
		printf("\nFailed to end mastermind\n");
		return -1;
	}
	return 0;
}

int overflow_control()
{
	if (start_mastermind() != 0) {
		printf("\nFailed to start mastermind\n");
		return -1;
	}

	if (write(mm_ctl_fd, "aaaaaaaaaa", 10) == -1) {
		printf("\nOverflow blocked\n\n");
		//successful
	} else {
		printf("Overflow block failed\n");
		return -1;
	}

	if (quit_mastermind() != 0) {
		printf("\nFailed to end mastermind\n");
		return -1;
	}
	return 0;
}

int game_inactive_read()
{
	char buf[10];
	if (read(mm_fd, buf, 4) == -1) {
		printf("\ninactive read blocked (pass)\n");
		return 0;
	} else
		return -1;
}

int game_inactive_write()
{
	if (write(mm_fd, "1234", 4) == -1) {
		printf("\ninactive write blocked (pass)\n");
		return 0;
	} else
		return -1;
}

int main(void)
{
	/* Here is an example of sending some data to CS421Net */
	cs421net_init();
	cs421net_send("4442", 4);

	/* YOUR CODE HERE */
	printf("MASTERMIND UNIT TESTING\n");

	mmap_test();
	happy_path_test();
	illegal_control();
	overflow_control();
	game_inactive_read();
	game_inactive_write();

	/*invalid_guess_digit();
	   invalid_guess_char();
	   large_guess();
	   small_guess();
	   large_result_read();
	   oob_resut_read();
	   overflow_user_view(); */
	return 0;
}
