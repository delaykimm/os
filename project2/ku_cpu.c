#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>
#include <linux/slab.h>

#define IDLE 0 // default value of "now"
#define MAX 50 // max size of queue - especially waiting queue

typedef struct jobobj // job type
{
	pid_t pid; // PID of process
	int job;   // remaining jobTime
	int prior; // priority
} job_t;

int first = 0; // front of queue
int last = 0;  // rear of queue
job_t wq[MAX] = { {0,0,0}, };  // queue as array

pid_t now = IDLE; // process pid currently using CPU (current process pid)
job_t nowJob; // remaining job of process current used CPU (remaining jobTime of current process
int timeslice = 30; // time slice[length] - for RR (default : 10 - 1 second)

// declare function prototype
void ku_init_queue(void);  // initialize the wait_Queue
job_t ku_pop(void);        // pop element from queue when bringing the job as next CPU occupying process
void ku_push(job_t new);   // put into the wait_Queue
bool ku_is_empty(void);    // check whether wait_Queue is empty
bool ku_is_new_id(pid_t check); // check whether newJob is already in wait_Queue
void ku_insertJob(job_t new);   // push the element into wait_Queue and sort in ascending order by jobTime
void ku_insertPrior(job_t new); // push the element into wait_Queue and sort in ascending order by priority
void ku_print_queue(void); // print the queue state for logging(debugging)

SYSCALL_DEFINE0(ku_cpu_init_queue) { // syscall for initiating wait_Queue
	printk(KERN_INFO "ku_cpu_init_queue called\n"); // 시스템 호출 진입 확인
	ku_init_queue();
	return 0;
}

// 시스템 호출 구현부
SYSCALL_DEFINE2(ku_cpu_fcfs, char*, name, int, jobTime){ // syscall for fcfs
    // store pid of current process as pid_t type
    job_t newJob = {current->pid, jobTime, 0};

    // register the process if virtual CPU is idle
    if (now == IDLE) now = newJob.pid;
    
    // If the process that sent the request is currently using virtual CPU
    if (now == newJob.pid) {
        // If the job has finished
        if (jobTime == 0) {
            printk("Process Finished: %s\n", name);
            // if queue is empty, virtual CPU becomes idle
            if (ku_is_empty()) now = IDLE;
            // if not, get next process from queue
            else now = (ku_pop()).pid;
        }
        else printk("Working: %s\n", name);
        //ku_print_queue(); // Print the queue state when the request is accepted
        return 0;
    }
    else {
        // if the request is not from currently handling process
        if (ku_is_new_id(newJob.pid)) { 
            ku_push(newJob);
        }
        printk("Working Denied: %s\n", name);
        //ku_print_queue(); // Print the queue state when the request is accepted
        return 1;
    }
}

SYSCALL_DEFINE2(ku_cpu_srtf, char*, name, int, jobTime){ // syscall for srtf
	// store pid of current process as pid_t type
	job_t newJob = {current->pid, jobTime, 0};

	// register the process if virtual CPU is idle
	if (now == IDLE) {
		now = newJob.pid;
		nowJob = newJob; 
	}
	// If the process that sent the request is currently using virtual CPU
	if (now == newJob.pid) {
		// If the job has finished
		if (jobTime == 0) {
			printk("Process Finished: %s\n", name);
			// if queue is empty, virtual CPU becomes idle
			if (ku_is_empty()) {
				now = IDLE;
			} else { // if not, get next process from queue
				job_t nextJob = ku_pop();
				now = nextJob.pid;
				nowJob = nextJob;
			}
        	} else {
			printk("Working: %s\n", name); 
        	}
        	//ku_print_queue(); // Print the queue state when the request is accepted
        	return 0;
	} else {
		if (ku_is_new_id(newJob.pid)){ // new process_ID
			if(newJob.job < nowJob.job){ // newJob is shorter, change process to newJob
				ku_insertJob(nowJob); // put nowJob in wait_Queue
				nowJob = newJob; // switch process to newJob
				now = nowJob.pid;
				printk("Working: %s\n", name);
				//ku_print_queue();
				return 0; // Job accepted
			} else {
				ku_insertJob(newJob);
			}
		}
		printk("Working Denied: %s\n", name); // Job rejected
		//ku_print_queue();
		return 1;
	}
}

SYSCALL_DEFINE2(ku_cpu_rr, char*, name, int, jobTime){ // syscall for round-robin
	job_t newJob = {current->pid, jobTime, 0};

	// register the process if virtual CPU is idle
	if (now == IDLE) {
		now = newJob.pid;
		nowJob = newJob; 
		timeslice = 30;
	}
	// If the process that sent the request is currently using virtual CPU
	if (now == newJob.pid) {
		// If the job has finished
		if (jobTime == 0) {
			printk("Process Finished: %s\n", name);
			// if queue is empty, virtual CPU becomes idle
			if (ku_is_empty()) {
				now = IDLE;
			} else { // if not, get next process from queue
				job_t nextJob = ku_pop();
				now = nextJob.pid;
				nowJob = nextJob;
			}
			timeslice = 30; // new round(reset timeslice)
			ku_print_queue();
			return 0; 
        	} 
		else if(timeslice == 0) { // if the round has finished
			printk("Turn Over ----> %s\n", name);
			ku_push(newJob); // put into wait_Queue
			nowJob = ku_pop(); // bring process to be executed in next round
			now = nowJob.pid;
			timeslice = 30;
			ku_print_queue();
			return 1;
		}
		else { // during the round, and job has not finished
			printk("Working: %s\n", name);
			timeslice--;
        	}
        	ku_print_queue(); // Print the queue state when the request is accepted
        	return 0;
	} else {  // other process is occupying CPU
		if (ku_is_new_id(newJob.pid)) ku_push(newJob); // not in wait_Queue, enqueue.
		printk("Working Denied: %s\n", name); // Job rejected
		ku_print_queue();
		return 1;
	}
}

SYSCALL_DEFINE3(ku_cpu_priority, char*, name, int, jobTime, int, priority){ // syscall for priority with preemption
	// store pid of current process as pid_t type
	job_t newJob = {current->pid, jobTime, priority};

	// register the process if virtual CPU is idle
	if (now == IDLE) {
		nowJob = newJob;
		now = nowJob.pid;
	}
	// If the process that sent the request is currently using virtual CPU
	if (now == newJob.pid) {
		// If the job has finished
		if (jobTime == 0) {
			printk("Process Finished: %s\n", name);
			// if queue is empty, virtual CPU becomes idle
			if (ku_is_empty()) {
				now = IDLE;
			} else { // if not, get next process from queue
				job_t nextJob = ku_pop();
				now = nextJob.pid;
				nowJob = nextJob;
			}
        	} else {
			printk("Working: %s\n", name);
        	}
        	//ku_print_queue(); // Print the queue state when the request is accepted
        	return 0;
	} else {
		if (ku_is_new_id(newJob.pid)){ // new process_ID
			if(newJob.prior < nowJob.prior){ // newJob is shorter, change process to newJob
				ku_insertPrior(nowJob); // put nowJob in wait_Queue
				nowJob = newJob; // switch process to newJob
				now = nowJob.pid;
				printk("Working: %s\n", name);
				//ku_print_queue();
				return 0; // Job accepted
			} else {
				ku_insertPrior(newJob);
			}
		}
		printk("Working Denied: %s\n", name); // Job rejected
		//ku_print_queue();
		return 1;
	}
}

// Initialize the queue
void ku_init_queue(void){
	first = 0;
	last = 0;
	printk(KERN_INFO "Queue initialized via syscall: first = %d, last = %d\n", first, last); // 디버깅 정보 추가
}

job_t ku_pop(void){
	if (ku_is_empty()){
	        job_t empty = {IDLE, 0, 0}; // default empty job
		return empty;
	}
	job_t popJob = wq[first];
	first = (first + 1) % MAX;
	return popJob; // pop element
}

void ku_push(job_t new){
	wq[last] = new;
	last = (last + 1) % MAX; // index to insert later input
}

bool ku_is_empty(void){
	return (first == last);
}

bool ku_is_new_id(pid_t check){
	int i ;
	for (i = first; i != last; i = (i + 1) % MAX){
		if (wq[i].pid == check) return false;
	}
	return true;
}

void ku_insertJob(job_t new){
	ku_push(new); // push new job
	int i;
	for (i = first; i != last; i = (i + 1) % MAX){
		if (wq[i].job > wq[(i + 1) % MAX].job) {  // sort by ascending order
			job_t tmp = wq[i];
			wq[i] = wq[(i + 1) % MAX];
			wq[(i + 1) % MAX] = tmp;
		}
	}
}

void ku_insertPrior(job_t new){
	ku_push(new); // push new job
	int i;
	for (i = first; i != last; i = (i + 1) % MAX){
		if (wq[i].prior > wq[(i + 1) % MAX].prior) { // sort by ascending order
			job_t tmp = wq[i];
			wq[i] = wq[(i + 1) % MAX];
			wq[(i + 1) % MAX] = tmp;
		}
	}
}

void ku_print_queue(void){
	printk(KERN_INFO "Queue State: front = %d, rear = %d\n", first, last);
	int i;
	for (i = first % MAX; i != last % MAX; i = (i + 1) % MAX){	
		printk(KERN_INFO "Queue[%d]: pid = %d, job = %d\n", i, wq[i].pid, wq[i].job);
	}
}
