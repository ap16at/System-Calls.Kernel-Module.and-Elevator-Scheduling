#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/linkage.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>

MODULE_LICENSE("GLP");

#define ENTRY_NAME "elevator"
#define ENTRY_SIZE 1000
#define PERMS 0644
#define PARENT NULL
#define MALLOC_FLAGS (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

/*************** DECLARATIONS ***************/

#define MAX_BUILDING_CAPACITY 100
#define MAX_LOAD 10
#define MOVE_WAIT 2
#define LOAD_WAIT 1
#define MAX_FLOOR 10
#define MIN_FLOOR 1

typedef enum state { OFFLINE, IDLE, LOADING, UP, DOWN } State;

typedef struct passenger {
	struct list_head list;
	int type; // 0 = grape, 1 = sheep, 2 = wolf
	int start_floor;
	int dest_floor;
} Passenger;

int reverse(State); // as boolean
int can_load(void); // as boolean
int will_load(int); // as boolean
int can_unload(void); // as boolean
int can_go_offline(void); // as boolean
int print_elevator_info(void);
void goto_next_floor(void);
void load_passengers(void);
void unload_passengers(void);
void goto_nearest_waiting(void);
void add_passenger_to_queue(int, int, int);
char * state_as_string(State);

struct task_struct *elevator_thread;
struct mutex queue_mutex;
struct mutex elevator_mutex;

static struct proc_dir_entry *proc_entry; // pointer to proc entry
static struct file_operations fops;
char *message;

State current_state = OFFLINE;
State previous_state = OFFLINE; // save the previous state to restore after LOADING
struct list_head passenger_queue[MAX_FLOOR]; // passengers on each floor
struct list_head elevator; // list of queues (floors)
int current_floor = 1;
int next_floor = 1;
int current_load = 0;
int total_serviced = 0;
int total_waiting = 0;
int type_frequency_onboard[3];
int waiting_on_floor[MAX_FLOOR];
int stop_requested = 0;
int current_max = 0;
int current_min = 11;

/*************** THREADING ***************/

static int elevator_run(void *data) {
	while (!kthread_should_stop()) {
		switch (current_state) {
			case OFFLINE:
				ssleep(LOAD_WAIT);
				break;
			case IDLE:
				// begin to go offline or find nearest passenger
				if (stop_requested) {
					current_state = DOWN;
				} else if (total_waiting > 0) {
					goto_nearest_waiting();
				}
				break;
			case LOADING:
				ssleep(LOAD_WAIT);
				unload_passengers();
				load_passengers();
				// determin the next state
				if (total_waiting == 0 && current_load == 0) {
					current_state = IDLE;
				} else {
					current_state = previous_state;
				}
				break;
			case UP:
				goto_next_floor();
				// determine the next state
				if (total_waiting == 0 && current_load == 0) {
					current_state = IDLE;
				} else if (reverse(UP)) {
					current_state = DOWN;
					next_floor = current_floor - 1;
					current_max = 0;
				} else {
					next_floor = current_floor + 1;
				}
				// load or unload if applicable
				if (can_load() || can_unload()) {
					previous_state = current_state;
					current_state = LOADING;
				}
				break;
			case DOWN:
				goto_next_floor();
				// determine the next state
				if (can_go_offline()) {
					current_state = OFFLINE;
					stop_requested = 0;
				} else if (total_waiting == 0 && current_load == 0) {
					current_state = IDLE;
				} else if (reverse(DOWN)) {
					current_state = UP;
					next_floor = current_floor + 1;
					current_min = 11;
				} else {
					next_floor = current_floor - 1;
				}
				// load or unload if applicable
				if (can_load() || can_unload()) {
					previous_state = current_state;
					current_state = LOADING;
				}
				break;
		}
	}
	return 0;
}

/*************** SYSCALLS ***************/

extern long(*STUB_start_elevator)(void);
long start_elevator(void) {
	if (current_state == OFFLINE) {
		current_state = IDLE;
		return 1;
	}
	return 0;
}

extern long(*STUB_issue_request)(int, int, int);
long issue_request(int start_floor, int dest_floor, int type) {
    int invalid_start = start_floor < MIN_FLOOR || start_floor > MAX_FLOOR;
    int invalid_dest = dest_floor < MIN_FLOOR || dest_floor > MAX_FLOOR;
    int invalid_type = type < 0 || type > 2;
	int is_offline = current_state == OFFLINE;
	int at_capacity = total_waiting == MAX_BUILDING_CAPACITY;

    if (at_capacity || invalid_start || invalid_dest || invalid_type || is_offline)
        return 1;

	if (start_floor != dest_floor)
		add_passenger_to_queue(start_floor, dest_floor, type);

	return 0;
}

extern long(*STUB_stop_elevator)(void);
long stop_elevator(void) {
	if (stop_requested || current_state == OFFLINE)
        return 1;

	stop_requested = 1;
	return 0;
}

/*************** HELPER FUNCTIONS ***************/

// change direction to stay within established bounds
int reverse(State direction) {
	int min_exists = 0, max_exists = 0;

	switch(direction) {
		case UP:
			if (stop_requested && current_load == 0) return 1; // can move to go offline
			max_exists = current_max != 0 && current_max <= current_floor;
			return current_floor == MAX_FLOOR || min_exists;
		case DOWN:
			min_exists = current_min != 11 && current_min >= current_floor;
			return current_floor == MIN_FLOOR || max_exists;
		default:
			return 0;
	}
}

int can_go_offline(void) {
	return stop_requested == 1
			&& current_load == 0
			&& current_floor == 1;
}

void goto_next_floor(void) {
	if (next_floor != current_floor) {
		ssleep(2);
		current_floor = next_floor;
	}
}

void goto_nearest_waiting(void) {
	int closest_floor = current_floor;
	int least_dist = MAX_FLOOR - MIN_FLOOR;
	int i;

	mutex_lock_interruptible(&queue_mutex);
	for (i = 1; i <= MAX_FLOOR; i++) {
		// passenger(s) waiting on this floor and is least distance
		if (waiting_on_floor[i - 1] > 0 && current_floor - i < least_dist) {
			least_dist = current_floor - i;
			closest_floor = i;
		}
	}

	// determine the next state
	if (closest_floor == current_floor) {
		previous_state = IDLE;
		current_state = LOADING;
	} else if (closest_floor < current_floor) {
		current_state = DOWN;
	} else {
		current_state = UP;
	}
	mutex_unlock(&queue_mutex);
}

void add_passenger_to_queue(int start_floor, int dest_floor, int type) {
	Passenger *passenger;
	total_waiting++;
	waiting_on_floor[start_floor - 1]++;

	passenger = kmalloc(sizeof(Passenger), MALLOC_FLAGS);
	passenger->start_floor = start_floor;
	passenger->dest_floor = dest_floor;
	passenger->type = type;

	mutex_lock_interruptible(&queue_mutex);
	list_add_tail(&passenger->list, &passenger_queue[start_floor - 1]);
	mutex_unlock(&queue_mutex);
}

// determines if passenger(s) on floor and elevator not at capacity 
int can_load(void) {
	Passenger *passenger;
	struct list_head *ptr;

	if (stop_requested)
		return 0;

	mutex_lock_interruptible(&queue_mutex);
	list_for_each(ptr, &passenger_queue[current_floor - 1]) {
		passenger = list_entry(ptr, Passenger, list);

		if (passenger->start_floor == current_floor && current_load < MAX_LOAD) {
			mutex_unlock(&queue_mutex);
			return 1;
		}
	}
	mutex_unlock(&queue_mutex);

	return 0;
}

// determines if there is a conflicting passenger onboard for "type"
int will_load(int type) {
	Passenger *passenger;
	struct list_head *ptr;

	mutex_lock_interruptible(&elevator_mutex);
	list_for_each(ptr, &elevator) {
		passenger = list_entry(ptr, Passenger, list);
		if (passenger->type == 1 && type == 0) {
			mutex_unlock(&elevator_mutex);
			return 0;
		}
		else if (passenger->type == 2 && type == 1) {
			mutex_unlock(&elevator_mutex);
			return 0;
		}
	}
	mutex_unlock(&elevator_mutex);

	return 1;
}

void load_passengers(void) {
	Passenger *passenger;
	struct list_head *ptr, *temp;

	mutex_lock_interruptible(&queue_mutex);
	list_for_each_safe(ptr, temp, &passenger_queue[current_floor - 1]) {
		passenger = list_entry(ptr, Passenger, list);

		// passenger can enter elevator
		if (passenger->start_floor == current_floor
			&& current_load < MAX_LOAD
			&& will_load(passenger->type))
		{
			Passenger *new_passenger;
			new_passenger = kmalloc(sizeof(Passenger), MALLOC_FLAGS);
			new_passenger->start_floor = passenger->start_floor;
			new_passenger->dest_floor = passenger->dest_floor;
			new_passenger->type = passenger->type;

			// set the new bounds if applicable
			if (new_passenger->dest_floor > current_max) {
				current_max = new_passenger->dest_floor;
			}
			if (new_passenger->dest_floor < current_min) {
				current_min = new_passenger->dest_floor;
			}

			list_add_tail(&new_passenger->list, &elevator);
			list_del(ptr);
			kfree(passenger);
			total_waiting--;
			waiting_on_floor[current_floor - 1]--;
			current_load++;
			type_frequency_onboard[new_passenger->type]++;
		}
	}
	mutex_unlock(&queue_mutex);
}

// determines if passenger(s) in elevator can exit on current floor
int can_unload(void) {
	Passenger *passenger;
	struct list_head *ptr;

	mutex_lock_interruptible(&elevator_mutex);
	list_for_each(ptr, &elevator) {
		passenger = list_entry(ptr, Passenger, list);

		if (passenger->dest_floor == current_floor) {
			mutex_unlock(&elevator_mutex);
			return 1;
		}
	}
	mutex_unlock(&elevator_mutex);

	return 0;
}

void unload_passengers(void)
{
	Passenger *passenger;
	struct list_head *ptr, *temp;

	mutex_lock_interruptible(&elevator_mutex);
	list_for_each_safe(ptr, temp, &elevator) {
		passenger = list_entry(ptr, Passenger, list);

		// passenger can exit elevator
		if (passenger->dest_floor == current_floor) {
			total_serviced++;
			current_load--;
			type_frequency_onboard[passenger->type]--;
			list_del(ptr);
			kfree(passenger);
		}
	}
	mutex_unlock(&elevator_mutex);
}

char * state_as_string(State state) {
	switch(state) {
		case OFFLINE:
			return "OFFLINE";
		case IDLE:
			return "IDLE";
		case LOADING:
			return "LOADING";
		case UP:
			return "UP";
		case DOWN:
			return "DOWN";
	}
	return "ERROR FOR STATE";
}

/*************** ELEVATOR PRINT ***************/

int print_elevator_info(void) {
	struct list_head *ptr;
	Passenger *passenger;
	int i, buffer = 0;
	int * buf = &buffer;

	// basic elevator info
	*buf += sprintf(message + *buf, "Elevator state: %s\n", state_as_string(current_state));
	*buf += sprintf(message + *buf, "Elevator status: %d wolves, %d sheep, %d grapes\n",
	type_frequency_onboard[2], type_frequency_onboard[1], type_frequency_onboard[0]);
	*buf += sprintf(message + *buf, "Current floor: %d\n", current_floor);
	*buf += sprintf(message + *buf, "Number of passengers: %d\n", current_load);
	*buf += sprintf(message + *buf, "Number of passengers waiting: %d\n", total_waiting);
	*buf += sprintf(message + *buf, "Number of passengers serviced: %d\n", total_serviced);

	// detailed floor breakdown
	mutex_lock_interruptible(&queue_mutex);
	for (i = MAX_FLOOR; i > 0; i--) {
		*buf += sprintf(message + *buf, "[");

		if (current_floor == i) {
			*buf += sprintf(message + *buf, "*");
		}

		*buf += sprintf(message + *buf, "] Floor %d:\t%d\t", i, waiting_on_floor[i - 1]);

		list_for_each(ptr, &passenger_queue[i - 1]) {
			passenger = list_entry(ptr, Passenger, list);
			switch(passenger->type) {
				case 0:
					*buf += sprintf(message + *buf, "G ");
					break;
				case 1:
					*buf += sprintf(message + *buf, "S ");
					break;
				case 2:
					*buf += sprintf(message + *buf, "W ");
					break;
			}
		}
		*buf += sprintf(message + *buf, "\n");
	}
	mutex_unlock(&queue_mutex);
	
	return 0;
}

/*************** PROCFILE ***************/

int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	message = kmalloc(sizeof(char) * ENTRY_SIZE, MALLOC_FLAGS);

	if (message == NULL) {
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}

	return print_elevator_info();
}

static ssize_t elevator_proc_read(
	struct file *sp_file,
	char __user *buf,
	size_t size,
	loff_t *offset
) {
	int len = strlen(message);
	printk(KERN_INFO "proc_read\n");

	if (*offset > 0 || size < len) // check if data already read and if space in user buffer
		return 0;

	if (copy_to_user(buf, message, len)) // send data to user buffer
		return -EFAULT;

	*offset = len;	// updates position
	printk(KERN_INFO "gave to user %s\n", message);
	return len; // returns # of chars read
}

int elevator_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

/*************** INIT AND EXIT ***************/

static int elevator_init(void) {
	int i;

	STUB_start_elevator = start_elevator;
	STUB_stop_elevator = stop_elevator;
	STUB_issue_request = issue_request;

	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;

  	for (i = 0; i < MAX_FLOOR; i++) {
		INIT_LIST_HEAD(&passenger_queue[i]);
	}
	INIT_LIST_HEAD(&elevator);

	mutex_init(&elevator_mutex);
	mutex_init(&queue_mutex);

	proc_entry = proc_create("elevator", 0666, NULL, &fops);
	if (proc_entry == NULL) {
		return -ENOMEM;
	}

	elevator_thread = kthread_run(elevator_run, NULL, "elevatorthread");
	if (IS_ERR(elevator_thread)) {
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(elevator_thread);
	}

	return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	mutex_destroy(&elevator_mutex);
	mutex_destroy(&queue_mutex);
	kthread_stop(elevator_thread);
	proc_remove(proc_entry);
}
module_exit(elevator_exit);
