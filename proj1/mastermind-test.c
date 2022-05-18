/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * This file is the unit test program for mastermind.c
 */

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <stdlib.h>

/*guess_letters:
  DESC: tries guessing a string of letters instead of numbers
  
  @return value: and int, 0 if success, -1 if fail
 */
int guess_letters()
{
	//open both fd
	char buf[100];

	printf("Opening /dev/mm for reading and writing\n");
	printf("Opening /dev/mm_ctl for writing\n");

	int mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	int mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}
	//starts mastermind game
	if (write(mm_ctl_fd, "start", 5) == -1)
		return -1;
	printf("Starting Mastermind Game via /dev/mm_ctl....\n");

	//begin unit test
	char guess[100];
	printf("type in some characters to guess (any length): ");
	if (scanf("%s", guess) == -1)
		return -1;
	printf("Attempting to guess \"%s\" instead of number.\n", guess);
	if (write(mm_fd, guess, sizeof(guess)) == -1)
		return -1;

	if (read(mm_fd, buf, 4) == -1)
		return -1;

	printf("And the output is: %s\n", buf);

	//ending game and closing file descriptors
	if (write(mm_ctl_fd, "quit", 4) == -1)
		return -1;
	printf("Ending Mastermind Game via /dev/mm_ctl....\n");

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

/*guess_moreNums:
  DESC: tries guessing more than 4 numbers

  @return value: and int, 0 if success -1 if fail
 */
int guess_moreNums()
{
	//open both fd
	char buf[100];

	printf("Opening /dev/mm for reading and writing\n");
	printf("Opening /dev/mm_ctl for writing\n");

	int mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	int mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}
	//starts mastermind game
	if (write(mm_ctl_fd, "start", 5) == -1)
		return -1;
	printf("Starting Mastermind Game via /dev/mm_ctl....\n");

	//begin unit test
	char *guess = "12345678";
	printf("Attempting to guess %s\n", guess);
	if (write(mm_fd, guess, sizeof(guess)) == -1)
		return -1;

	if (read(mm_fd, buf, 4) == -1)
		return -1;

	printf("And the output is: %s\n", buf);

	//ending game and closing file descriptors
	if (write(mm_ctl_fd, "quit", 4) == -1)
		return -1;
	printf("Ending Mastermind Game via /dev/mm_ctl....\n");

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

/*guess_lessNums:
  DESC: tries guessing less than 4 numbers
  
  @return value: and int, 0 if success, -1 if fail
 */
int guess_lessNums()
{
	//open both fd
	char buf[100];

	printf("Opening /dev/mm for reading and writing\n");
	printf("Opening /dev/mm_ctl for writing\n");

	int mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	int mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}
	//starts mastermind game
	if (write(mm_ctl_fd, "start", 5) == -1)
		return -1;
	printf("Starting Mastermind Game via /dev/mm_ctl....\n");

	//begin unit test
	char *guess = "56";
	printf("Attempting to guess %s\n", guess);
	if (write(mm_fd, guess, sizeof(guess)) == -1)
		return -1;

	if (read(mm_fd, buf, 4) == -1)
		return -1;

	printf("And the output is: %s\n", buf);

	//ending game and closing file descriptors
	if (write(mm_ctl_fd, "quit", 4) == -1)
		return -1;
	printf("Ending Mastermind Game via /dev/mm_ctl....\n");

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

/*test_start:
  DESC: tries to open start using the string "started"
  
  @return value: and int, 0 if success, -1 if fail
 */
int test_start()
{
	//open both fd
	//char buf[100];

	printf("Opening /dev/mm for reading and writing\n");
	printf("Opening /dev/mm_ctl for writing\n");

	int mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	int mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}
	//start unit test
	printf("Attempting to write \"started\" to /dev/mm_ctl....\n");

	if (write(mm_ctl_fd, "started", 7) == -1) {
		printf("Unable to write \"started\" to the device.");
		//successful
	} else {
		//need to quit
		if (write(mm_ctl_fd, "quit", 4) == -1)
			return -1;
		printf("Ending Mastermind Game via /dev/mm_ctl....\n");
	}

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

/*test_quit:
  DESC: tries to write quit  using the string "quiting"
  
  @return value: and int, 0 if success, -1 if fail
 */
int test_quit()
{
	//open both fd
	//char buf[100];

	printf("Opening /dev/mm for reading and writing\n");
	printf("Opening /dev/mm_ctl for writing\n");

	int mm_fd = open("/dev/mm", O_RDWR);
	if (mm_fd == -1) {
		printf("Could not open /dev/mm\n");
		return mm_fd;
	}
	int mm_ctl_fd = open("/dev/mm_ctl", O_WRONLY);
	if (mm_ctl_fd == -1) {
		printf("Could not open /dev/mm_ctl\n");
		return mm_ctl_fd;
	}
	//starts mastermind game
	if (write(mm_ctl_fd, "start", 5) == -1)
		return -1;
	printf("Starting Mastermind Game via /dev/mm_ctl....\n");

	//start unit test
	printf("Attempting to write \"quiting\" to /dev/mm_ctl....\n");

	if (write(mm_ctl_fd, "quiting", 7) == -1) {
		printf("Unable to write \"quiting\" to the device.");
		//successful
	} else {
		//need to quit
		if (write(mm_ctl_fd, "quit", 4) == -1)
			return -1;
		printf("Ending Mastermind Game via /dev/mm_ctl....\n");
	}

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

/*Main Testing loop (has the user pick what test they would like to preform)*/
int main(void)
{

	printf("MASTERMIND UNIT TESTING\n");

	int pick = 0;
	int outcome;

	do {
		outcome = 0;
		printf
		    ("\nPlease choose a number from the menu which you would like to test:\n");
		printf("\t1.) try guessing letters instead of numbers.\n");
		printf("\t2.) try guessing more than 4 numbers.\n");
		printf("\t3.) try guessing less than 4 numbers.\n");
		printf
		    ("\t4.) test writing \"started\" to /dev/mm_ctl to start.\n");
		printf
		    ("\t5.) test writing \"quiting\" to /dev/mm_ctl to quit.\n");

		if (scanf("%d", &pick) == -1)
			return -1;

		switch (pick) {

		case 1:
			outcome = guess_letters();
			break;

		case 2:
			outcome = guess_moreNums();
			break;

		case 3:
			outcome = guess_lessNums();
			break;

		case 4:
			outcome = test_start();
			break;

		case 5:
			outcome = test_quit();
			break;

		default:
			if (pick != -1)
				printf("\nNot a valid choice");
			break;
		}
		if (outcome == -1 && pick != -1)
			printf("\nTEST %d FAILED\n", pick);
		else
			printf("\nTEST %d RAN SUCESSFUL\n", pick);
		if (pick != -1)
			printf("\n\npick again or -1 to quit: ");
	} while (pick != -1);

	return 0;
}
