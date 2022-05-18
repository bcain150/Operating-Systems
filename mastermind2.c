/**
 * @Author Brendan Cain (bcain1@umbc.edu)
 * This file creates 2 separte miscellaneous kernel diveces to
 * implement the mastermind game (2nd verison)
 * (one to start/quit and other for control)
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

#define pr_fmt(fmt) "mastermind2: " fmt

#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/vmalloc.h>

#include "nf_cs421net.h"

#define NUM_PEGS 4
#define NUM_COLORS 6

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

/* Copy mm_read(), mm_write(), mm_mmap(), and mm_ctl_write(), along
 * with all of your global variables and helper functions here.
 */
/* Part 1: YOUR CODE HERE */

#define USER_VIEW_SIZE 4096

/**Sets the limit of the number of colors*/
static int max_numbers;

/*tracks number of games started*/
static int game_count = 0;

/* tracks the number of times the code was changed*/
static int code_changed = 0;

/*tracks the number of invalid attempts to code change*/
static int invalid_attempts = 0;

/*tracks number of currently active games*/
static int active_games = 0;

/*prototypes*/
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

/*holds player global variables*/
struct mm_game {
  /** true if user is in the middle of a game */
	bool game_active;
  /** code that player is trying to guess */
	int target_code[NUM_PEGS];
  /** tracks number of guesses user has made */
	unsigned num_guesses;
  /** result of most recent user guess */
	char last_result[4];
  /** buffer that records all of user's guesses and their results */
	char *user_view;
  /** use_view offset for the current placement of last_result*/
	size_t uv_off;
  /** identifies the kthread process number*/
	kuid_t k_id;
	/* linked list pointer */
	struct list_head list;
};

static LIST_HEAD(game_globals);

/*spinlock to protect global variables*/
static DEFINE_SPINLOCK(mm_spinlock);

/**
 * mm_find_game() - function that returns global vars given a uid
 *-if global is unallocated then allocate it with kmalloc
 *-set uid as long as globals are alloc'd
 *-doesn't use locking so calling function MUST hold lock before call.
 *@uid: process id for the game youre accessing
 *
 *RETURN: the pointer to a mm_game struct, or 0 if it cant alloc
 */
static struct mm_game *mm_find_game(kuid_t uid)
{
	struct mm_game *retval;

	list_for_each_entry(retval, &game_globals, list) {
		if (retval == NULL) {
			retval = kmalloc(sizeof(*retval), GFP_KERNEL);
			if (!retval) {
				pr_err
				    ("Could not allocate memory for game_globals\n");
				return 0;
			}
			retval->user_view = vmalloc(PAGE_SIZE);
			if (!retval->user_view) {
				pr_err
				    ("Could not allocate memory for user_view\n");
				return 0;
			}
			retval->k_id = uid;
			list_add(&retval->list, &game_globals);
			active_games++;
			return retval;
		}

		/*compare uid */
		if (uid_eq(retval->k_id, uid)) {
			return retval;
		}
	}

	retval = kmalloc(sizeof(*retval), GFP_KERNEL);
	if (!retval) {
		pr_err("Could not allocate memory for game_globals\n");
		return 0;
	}
	retval->user_view = vmalloc(PAGE_SIZE);
	if (!retval->user_view) {
		pr_err("Could not allocate memory for user_view\n");
		return 0;
	}
	retval->k_id = uid;
	list_add(&retval->list, &game_globals);
	active_games++;
	return retval;
}

/**
 * mm_free_games() - frees all game memory
 *
 *@void: process id for the game youre accessing
 *
 * return void
 */
static void mm_free_games(void)
{
	struct list_head *game_vars;
	struct list_head *tmp;
	struct mm_game *cont;

	list_for_each_safe(game_vars, tmp, &game_globals) {
		cont = container_of(game_vars, struct mm_game, list);
		if (cont->user_view != NULL) {
			vfree(cont->user_view);
			cont->user_view = NULL;
		}
		kfree(cont);
		cont = NULL;
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
	unsigned long flags;
	size_t num_bytes = count;
	char *q = "????";
	struct mm_game *game_vars;

	spin_lock_irqsave(&mm_spinlock, flags);
	game_vars = mm_find_game(current_uid());
	spin_unlock_irqrestore(&mm_spinlock, flags);

	if (game_vars == 0)
		return 0;

	if (*ppos > 0)
		return 0;

	spin_lock_irqsave(&mm_spinlock, flags);
	if (game_vars->game_active) {
		spin_unlock_irqrestore(&mm_spinlock, flags);

		if (NUM_PEGS > *ppos) {

			if (NUM_PEGS - *ppos < count)
				num_bytes = NUM_PEGS - *ppos;
			spin_lock_irqsave(&mm_spinlock, flags);
			if (copy_to_user
			    (ubuf + *ppos, game_vars->last_result,
			     num_bytes) != 0) {
				spin_unlock_irqrestore(&mm_spinlock, flags);
				return 0;
			}
			spin_unlock_irqrestore(&mm_spinlock, flags);
			*ppos += num_bytes;
			return num_bytes;
		}
		return 0;

	} else {
		spin_unlock_irqrestore(&mm_spinlock, flags);
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
	unsigned long flags;
	size_t i;
	int g[NUM_PEGS];
	char guess[NUM_PEGS];
	unsigned black;
	unsigned white;
	struct mm_game *game_vars;

	spin_lock_irqsave(&mm_spinlock, flags);
	game_vars = mm_find_game(current_uid());
	spin_unlock_irqrestore(&mm_spinlock, flags);

	if (game_vars == 0)
		return -ENOMEM;

	spin_lock_irqsave(&mm_spinlock, flags);
	if (game_vars->game_active && count >= NUM_PEGS) {
		spin_unlock_irqrestore(&mm_spinlock, flags);
		if (copy_from_user(guess, ubuf, NUM_PEGS) != 0)
			return -1;
		spin_lock_irqsave(&mm_spinlock, flags);
		for (i = 0; i < NUM_PEGS; i++) {
			g[i] = guess[i] - '0';
			if (g[i] > max_numbers)
				return -EINVAL;
		}

		/*get number of black and white pegs */
		mm_num_pegs(game_vars->target_code, g, &black, &white);

		/*update last result */
		game_vars->last_result[1] = black + '0';
		game_vars->last_result[3] = white + '0';

		/*updates num guesses */
		game_vars->num_guesses++;

		/*update user_view */
		game_vars->uv_off +=
		    scnprintf(game_vars->user_view + game_vars->uv_off,
			      PAGE_SIZE, "Guess %d: %d%d%d%d | %c%c%c%c\n",
			      game_vars->num_guesses, g[0], g[1], g[2], g[3],
			      game_vars->last_result[0],
			      game_vars->last_result[1],
			      game_vars->last_result[2],
			      game_vars->last_result[3]);
		pr_info("%s\n", game_vars->user_view);

		if (black == NUM_PEGS) {
			game_vars->uv_off +=
			    scnprintf(game_vars->user_view +
				      game_vars->uv_off, PAGE_SIZE,
				      "You won the game!\n");
			game_vars->game_active = false;
		}
		spin_unlock_irqrestore(&mm_spinlock, flags);
		return count;
	} else {
		spin_unlock_irqrestore(&mm_spinlock, flags);
		return -EINVAL;
	}
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
	unsigned long flags;
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
	unsigned long page;
	struct mm_game *game_vars;

	spin_lock_irqsave(&mm_spinlock, flags);
	game_vars = mm_find_game(current_uid());

	if (game_vars == 0)
		return -ENOMEM;

	page = vmalloc_to_pfn(game_vars->user_view);
	spin_unlock_irqrestore(&mm_spinlock, flags);

	if (size > PAGE_SIZE) {
		return -EIO;
	}
	vma->vm_pgoff = 0;
	vma->vm_page_prot = PAGE_READONLY;
	if (remap_pfn_range(vma, vma->vm_start, page, size, vma->vm_page_prot)) {

		return -EAGAIN;
	}
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
	unsigned long flags;
	const size_t max = 10;
	char input[10];
	const char *go = "start";
	const char *stop = "quit";
	const char *colors = "colors X";
	size_t i;
	bool valid = true;
	bool start;
	int num = 0;
	struct mm_game *game_vars;

	spin_lock_irqsave(&mm_spinlock, flags);
	game_vars = mm_find_game(current_uid());
	spin_unlock_irqrestore(&mm_spinlock, flags);

	if (game_vars == 0)
		return -ENOMEM;

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
	} else if (count > 0 && colors[0] == input[0]) {
		for (i = 1; i < count; i++) {
			if (colors[i] != input[i] || i == 7) {
				if (i == 7) {
					num = input[i] - '0';
					if (num < 2 || num > 9)
						return -EINVAL;
				} else {
					valid = false;
					break;
				}
			}
		}
		if (valid) {
			if (capable(CAP_SYS_ADMIN)) {
				spin_lock_irqsave(&mm_spinlock, flags);
				max_numbers = num;
				spin_unlock_irqrestore(&mm_spinlock, flags);
				return count;
			} else
				return -EACCES;
		} else
			return -EINVAL;
	} else
		return -EINVAL;

	/*if the input was start */
	spin_lock_irqsave(&mm_spinlock, flags);
	if (start) {

		game_count++;

		game_vars->target_code[0] = 4;
		game_vars->target_code[1] = 2;
		game_vars->target_code[2] = 1;
		game_vars->target_code[3] = 1;

		game_vars->num_guesses = 0;
		for (i = 0; i < USER_VIEW_SIZE; i++)
			game_vars->user_view[i] = '\0';
		game_vars->game_active = true;
		game_vars->last_result[0] = 'B';
		game_vars->last_result[1] = '-';
		game_vars->last_result[2] = 'W';
		game_vars->last_result[3] = '-';

		/*reset userview byte offset */
		game_vars->uv_off = 0;

	} else {		/*if the input was quit */
		game_vars->game_active = false;
		active_games--;
	}
	spin_unlock_irqrestore(&mm_spinlock, flags);

	return count;
}

/**
 * cs421net_top() - top-half of CS421Net ISR
 * @irq: IRQ that was invoked (ignored)
 * @cookie: Pointer to data that was passed into
 * request_threaded_irq() (ignored)
 *
 * If @irq is CS421NET_IRQ, then wake up the bottom-half. Otherwise,
 * return IRQ_NONE.
 */
static irqreturn_t cs421net_top(int irq, void *cookie)
{
	if (irq == CS421NET_IRQ)
		return IRQ_WAKE_THREAD;
	return IRQ_NONE;
}

/**
 * cs421net_bottom() - bottom-half to CS421Net ISR
 * @irq: IRQ that was invoked (ignore)
 * @cookie: Pointer that was passed into request_threaded_irq()
 * (ignored)
 *
 * Fetch the incoming packet, via cs421net_get_data(). If:
 *   1. The packet length is exactly equal to four bytes, and
 *   2. If all characters in the packet are valid ASCII representation
 *      of valid digits in the code, then
 * Set the target code to the new code, and increment the number of
 * tymes the code was changed remotely. Otherwise, ignore the packet
 * and increment the number of invalid change attempts.
 *
 * Because the payload is dynamically allocated, free it after parsing
 * it.
 *
 * During Part 5, update this function to change all codes for all
 * active games.
 *
 * Remember to add appropriate spin lock calls in this function.
 *
 * <em>Caution: The incoming payload is NOT a string; it is not
 * necessarily null-terminated.</em> You CANNOT use strcpy() or
 * strlen() on it!
 *
 * Return: always IRQ_HANDLED
 */
static irqreturn_t cs421net_bottom(int irq, void *cookie)
{
	/* Part 4: YOUR CODE HERE */
	size_t len = 0;
	char *data;
	int i = 0;
	int num = 0;
	char code[4];
	unsigned long flags;
	struct mm_game *game_vars;

	/*spin_lock_irqsave(&mm_spinlock, flags);
	   game_vars = mm_find_game(current_uid());
	   spin_unlock_irqrestore(&mm_spinlock, flags);
	   if(game_vars == 0)
	   return IRQ_HANDLED;
	 */

	data = cs421net_get_data(&len);

	if (len == 4) {
		spin_lock_irqsave(&mm_spinlock, flags);
		for (i = 0; i < 4; i++) {
			num = data[i] - '0';

			if (num < 2 || num > max_numbers) {
				kfree(data);
				invalid_attempts++;
				spin_unlock_irqrestore(&mm_spinlock, flags);
				return IRQ_HANDLED;
			}
			code[i] = num;
		}
		/*iterates over linked list and changes the code for each */
		if (active_games == 0) {
			kfree(data);
			return IRQ_HANDLED;
		}
		list_for_each_entry(game_vars, &game_globals, list) {
			for (i = 0; i < 4; i++) {
				game_vars->target_code[i] = code[i];
			}
		}
		code_changed++;
		spin_unlock_irqrestore(&mm_spinlock, flags);
	} else {
		invalid_attempts++;
	}

	kfree(data);
	return IRQ_HANDLED;
}

/**
 * mm_stats_show() - callback invoked when a process reads from
 * /sys/devices/platform/mastermind/stats
 *
 * @dev: device driver data for sysfs entry (ignored)
 * @attr: sysfs entry context (ignored)
 * @buf: destination to store game statistics
 *
 * Write to @buf, up to PAGE_SIZE characters, a human-readable message
 * containing these game statistics:
 *  - Number of colors (range of digits in target code)
 *   - Number of started games
 *   - Number of active games
 *   - Number of valid network messages (see Part 4)
 *   - Number of invalid network messages (see Part 4)
 * Note that @buf is a normal character buffer, not a __user
 * buffer. Use scnprintf() in this function.
 *
 * @return Number of bytes written to @buf, or negative on error.
 */
static ssize_t mm_stats_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	/* Part 3: YOUR CODE HERE */
	int numbytes = 0;
	unsigned long flags;

	spin_lock_irqsave(&mm_spinlock, flags);
	numbytes = scnprintf(buf, PAGE_SIZE, "CS421 Mastermind Stats\n\
Number of colors: %d\n\
Number of started games: %d\n\
Number of active games: %d\n\
Number of valid code changes: %d\n\
Number of invalid network messages: %d\n", max_numbers, game_count, active_games, code_changed, invalid_attempts);

	spin_unlock_irqrestore(&mm_spinlock, flags);

	pr_info("numbytes = %d", numbytes);
	return numbytes;
}

static DEVICE_ATTR(stats, S_IRUGO, mm_stats_show, NULL);

/**
 * mastermind_probe() - callback invoked when this driver is probed
 * @pdev platform device driver data
 *
 * Return: 0 on successful probing, negative on error
 */
static int mastermind_probe(struct platform_device *pdev)
{
	/* Merge the contents of your original mastermind_init() here. */
	/* Part 1: YOUR CODE HERE */

	int err;
	unsigned long flags;

	spin_lock_irqsave(&mm_spinlock, flags);
	pr_info("Initializing the game.\n");

	/* YOUR CODE HERE */
	max_numbers = NUM_COLORS;
	spin_unlock_irqrestore(&mm_spinlock, flags);

	cs421net_enable();

	err = misc_register(&mm_device);	/*registers misc character device returns error if failed */
	if (err)
		goto fail_mm;
	err = misc_register(&mm_ctl_device);	/*registers mm_ctl_device */
	if (err)
		goto fail_mm_ctl;
	err = device_create_file(&pdev->dev, &dev_attr_stats);
	if (err) {
		pr_err("Could not create sysfs entry\n");
		goto fail_create_file;
	}
	err =
	    request_threaded_irq(CS421NET_IRQ, cs421net_top, cs421net_bottom, 0,
				 "mm_Set_Code", NULL);
	if (err) {
		pr_err("Was unable to register Interrupt handler\n");
		goto fail_irq_reg;
	}
	return err;

	//failed registrations
fail_irq_reg:device_remove_file(&pdev->dev, &dev_attr_stats);
fail_create_file:misc_deregister(&mm_ctl_device);
fail_mm_ctl:misc_deregister(&mm_device);
fail_mm:return err;

}

/**
 * mastermind_remove() - callback when this driver is removed
 * @pdev platform device driver data
 *
 * Return: Always 0
 */
static int mastermind_remove(struct platform_device *pdev)
{
	/* Merge the contents of your original mastermind_exit() here. */
	/* Part 1: YOUR CODE HERE */
	unsigned long flags;

	spin_lock_irqsave(&mm_spinlock, flags);
	pr_info("Freeing resources.\n");
	mm_free_games();
	spin_unlock_irqrestore(&mm_spinlock, flags);

	/* YOUR CODE HERE */
	cs421net_disable();
	misc_deregister(&mm_device);	/*undo's the registration from init */
	misc_deregister(&mm_ctl_device);	/*undo's the mm_ctl registration from init */
	device_remove_file(&pdev->dev, &dev_attr_stats);
	free_irq(CS421NET_IRQ, NULL);

	return 0;
}

static struct platform_driver cs421_driver = {
	.driver = {
		   .name = "mastermind",
		   },
	.probe = mastermind_probe,
	.remove = mastermind_remove,
};

static struct platform_device *pdev;

/**
 * cs421_init() -  create the platform driver
 * This is needed so that the device gains a sysfs group.
 *
 * <strong>You do not need to modify this function.</strong>
 */
static int __init cs421_init(void)
{
	pdev = platform_device_register_simple("mastermind", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	return platform_driver_register(&cs421_driver);
}

/**
 * cs421_exit() - remove the platform driver
 * Unregister the driver from the platform bus.
 *
 * <strong>You do not need to modify this function.</strong>
 */
static void __exit cs421_exit(void)
{
	platform_driver_unregister(&cs421_driver);
	platform_device_unregister(pdev);
}

module_init(cs421_init);
module_exit(cs421_exit);

MODULE_DESCRIPTION("CS421 Mastermind Game++");
MODULE_LICENSE("GPL");
