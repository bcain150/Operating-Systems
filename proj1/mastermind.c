/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * This file creates 2 separte miscellaneous kernel diveces to
 * implement the mastermind game (one to start/quit and other for control)
 */

/*
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments.  See
 * ~/linux/Documentation/kernel-doc-nano-HOWTO.txt for details.
 */

/*
 * Getting compilation warnings?  The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read ~/linux/Documentation/CodingStyle to ensure your project
 * compiles without warnings.
 */

#define pr_fmt(fmt) "MM: " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define NUM_PEGS 4
#define NUM_COLORS 6
#define USER_VIEW_SIZE 4096

/** true if user is in the middle of a game */
static bool game_active;

/** code that player is trying to guess */
static int target_code[NUM_PEGS];

/** tracks number of guesses user has made */
static unsigned num_guesses;

/** result of most recent user guess */
static char last_result[4];

/** buffer that records all of user's guesses and their results */
static char *user_view;

/** use_view offset for the current placement of last_result*/
static size_t uv_off;

//prototypes
static ssize_t mm_read(struct file *filp, char __user * ubuf, size_t count,
		       loff_t * ppos);
static ssize_t mm_write(struct file *filp, const char __user * ubuf,
			size_t count, loff_t * ppos);
static int mm_mmap(struct file *filp, struct vm_area_struct *vma);
static ssize_t mm_ctl_write(struct file *filp, const char __user * ubuf,
			    size_t count, loff_t * ppos);

/** handles all /dev/mm file operations */
static const struct file_operations mm_fops = {
	.read = mm_read,
	.write = mm_write,
	.mmap = mm_mmap
};

static struct miscdevice mm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mm",
	.fops = &mm_fops,
	.mode = 0666
};

static const struct file_operations mm_ctl_fops = {
	.write = mm_ctl_write
};

static struct miscdevice mm_ctl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mm_ctl",
	.fops = &mm_ctl_fops,
	.mode = 0666
};

/**
 * mm_num_pegs() - calculate number of black pegs and number of white pegs
 * @target: target code, up to NUM_PEGS elements
 * @guess: user's guess, up to NUM_PEGS elements
 * @num_black: *OUT* parameter, to store calculated number of black pegs
 * @num_white: *OUT* parameter, to store calculated number of white pegs
 *
 * You do not need to modify this function.
 *
 */
static void mm_num_pegs(int target[], int guess[], unsigned *num_black,
			unsigned *num_white)
{
	size_t i;
	size_t j;
	bool peg_black[NUM_PEGS];
	bool peg_used[NUM_PEGS];

	*num_black = 0;
	for (i = 0; i < NUM_PEGS; i++) {
		if (guess[i] == target[i]) {
			(*num_black)++;
			peg_black[i] = true;
			peg_used[i] = true;
		} else {
			peg_black[i] = false;
			peg_used[i] = false;
		}
	}

	*num_white = 0;
	for (i = 0; i < NUM_PEGS; i++) {
		if (peg_black[i])
			continue;
		for (j = 0; j < NUM_PEGS; j++) {
			if (guess[i] == target[j] && !peg_used[j]) {
				peg_used[j] = true;
				(*num_white)++;
				break;
			}
		}
	}
}

/**
 * mm_read() - callback invoked when a process reads from
 * /dev/mm
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store output
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * Write to @ubuf the last result of the game, offset by
 * @ppos. Copy the lesser of @count and (string length of @last_result
 * - *@ppos). Then increment the value pointed to by @ppos by the
 * number of bytes copied. If @ppos is greater than or equal to the
 * length of @last_result, then copy nothing.
 *
 * If no game is active, instead copy to @ubuf up to four '?'
 * characters.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t mm_read(struct file *filp, char __user * ubuf, size_t count,
		       loff_t * ppos)
{
	size_t num_bytes = count;
	char *q = "????";

	if (*ppos > 0)
		return 0;

	if (game_active) {
		if (NUM_PEGS > *ppos) {

			if (NUM_PEGS - *ppos < count)
				num_bytes = NUM_PEGS - *ppos;
			if (copy_to_user(ubuf + *ppos, last_result, num_bytes)
			    != 0)
				return 0;
			*ppos += num_bytes;
			return num_bytes;
		}
		return 0;
	} else {

		if (copy_to_user(ubuf + *ppos, q, NUM_PEGS) != 0) {
			return 0;
		}
		*ppos += NUM_PEGS;
		return NUM_PEGS;
	}
}

/**
 * mm_write() - callback invoked when a process writes to /dev/mm
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * If the user is not currently playing a game, then return -EINVAL.
 *
 * If @count is less than NUM_PEGS, then return -EINVAL. Otherwise,
 * interpret the first NUM_PEGS characters in @ubuf as the user's
 * guess. Calculate how many are in the correct value and position,
 * and how many are simply the correct value. Then update
 * @num_guesses, @last_result, and @user_view.
 *
 * <em>Caution: @ubuf is NOT a string; it is not necessarily
 * null-terminated.</em> You CANNOT use strcpy() or strlen() on it!
 *
 * Return: @count, or negative on error
 */
static ssize_t mm_write(struct file *filp, const char __user * ubuf,
			size_t count, loff_t * ppos)
{
	size_t i;
	int g[NUM_PEGS];
	char guess[NUM_PEGS];
	unsigned black;
	unsigned white;

	if (game_active && count >= NUM_PEGS) {
		if (copy_from_user(guess, ubuf, NUM_PEGS) != 0)
			return -1;

		for (i = 0; i < NUM_PEGS; i++) {
			g[i] = guess[i] - '0';
		}

		/*get number of black and white pegs */
		mm_num_pegs(target_code, g, &black, &white);

		/*update last result */
		last_result[1] = black + '0';
		last_result[3] = white + '0';

		/*updates num guesses */
		num_guesses++;

		/*update user_view */
		uv_off +=
		    scnprintf(user_view + uv_off, PAGE_SIZE,
			      "Guess %d: %d%d%d%d | %c%c%c%c\n", num_guesses,
			      g[0], g[1], g[2], g[3], last_result[0],
			      last_result[1], last_result[2], last_result[3]);
		pr_info("%s\n", user_view);

		return count;
	} else
		return -EINVAL;
}

/**
 * mm_mmap() - callback invoked when a process mmap()s to /dev/mm
 * @filp: process's file object that is mapping to this device (ignored)
 * @vma: virtual memory allocation object containing mmap() request
 *
 * Create a read-only mapping from kernel memory (specifically,
 * @user_view) into user space.
 *
 * Code based upon
 * <a href="http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/">http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/</a>
 *
 * You do not need to modify this function.
 *
 * Return: 0 on success, negative on error.
 */
static int mm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
	unsigned long page = vmalloc_to_pfn(user_view);
	if (size > PAGE_SIZE)
		return -EIO;
	vma->vm_pgoff = 0;
	vma->vm_page_prot = PAGE_READONLY;
	if (remap_pfn_range(vma, vma->vm_start, page, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

/**
 * mm_ctl_write() - callback invoked when a process writes to
 * /dev/mm_ctl
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Copy the contents of @ubuf, up to the lesser of @count and 8 bytes,
 * to a temporary location. Then parse that character array as
 * following:
 *
 *  start - Start a new game. If a game was already in progress, restart it.
 *  quit  - Quit the current game. If no game was in progress, do nothing.
 *
 * If the input is neither of the above, then return -EINVAL.
 *
 * <em>Caution: @ubuf is NOT a string;</em> it is not necessarily
 * null-terminated, nor does it necessarily have a trailing
 * newline. You CANNOT use strcpy() or strlen() on it!
 *
 * Return: @count, or negative on error
 */
static ssize_t mm_ctl_write(struct file *filp, const char __user * ubuf,
			    size_t count, loff_t * ppos)
{

	const size_t max = 8;
	char input[8];
	const char *go = "start";
	const char *stop = "quit";
	size_t i;
	bool valid = true;
	bool start;

	/*copies user buffer to temp buffer inorder to parse */
	if (count > max)
		count = max;
	if (copy_from_user(input, ubuf, count) != 0)
		return -1;

	/*check to see if input is valid and whether it was start or quit */
	if (count > 0 && go[0] == input[0]) {	/*if start */
		for (i = 1; i < count; i++) {
			if (go[i] != input[i]) {
				valid = false;
				break;
			}
		}
		if (valid)
			start = true;
		else
			return -EINVAL;
	} else if (count > 0 && stop[0] == input[0]) {
		for (i = 1; i < count; i++) {
			if (stop[i] != input[i]) {
				valid = false;
				break;
			}
		}
		if (valid)
			start = false;
		else
			return -EINVAL;
	} else
		return -EINVAL;

	/*if the input was start */
	if (start) {
		target_code[0] = 4;
		target_code[1] = 2;
		target_code[2] = 1;
		target_code[3] = 1;

		num_guesses = 0;
		for (i = 0; i < USER_VIEW_SIZE; i++)
			user_view[i] = '\0';
		game_active = true;
		last_result[0] = 'B';
		last_result[1] = '-';
		last_result[2] = 'W';
		last_result[3] = '-';

		/*reset userview byte offset */
		uv_off = 0;
	} else {		/*if the input was quit */
		game_active = false;
	}

	return count;
}

/**
 * mastermind_init() - entry point into the Mastermind kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init mastermind_init(void)
{
	int err;

	pr_info("Initializing the game.\n");
	user_view = vmalloc(PAGE_SIZE);
	if (!user_view) {
		pr_err("Could not allocate memory\n");
		return -ENOMEM;
	}

	/* YOUR CODE HERE */
	err = misc_register(&mm_device);	/*registers misc character device returns error if failed */
	if (err)
		goto fail_mm;
	err = misc_register(&mm_ctl_device);	/*registers mm_ctl_device */
	if (err)
		goto fail_mm_ctl;
	return 0;

	//failed registrations
fail_mm_ctl:misc_deregister(&mm_device);
fail_mm:return err;
}

/**
 * mastermind_exit() - called by kernel to clean up resources
 */
static void __exit mastermind_exit(void)
{
	pr_info("Freeing resources.\n");
	vfree(user_view);

	/* YOUR CODE HERE */
	misc_deregister(&mm_device);	/*undo's the registration from init */
	misc_deregister(&mm_ctl_device);	/*undo's the mm_ctl registration from init */
}

module_init(mastermind_init);
module_exit(mastermind_exit);

MODULE_DESCRIPTION("CS421 Mastermind Game");
MODULE_LICENSE("GPL");
