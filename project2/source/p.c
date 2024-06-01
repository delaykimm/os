#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ku_cpu_fcfs 337 // define syscall number
#define ku_cpu_srtf 338 
#define ku_cpu_rr 339
#define ku_cpu_priority 340
#define ku_cpu_init_queue 341 // define syscall number for initializing queue

int main(int argc,char ** argv){
	int jobTime;   // the execution duration
	int delayTime; // the execution delay
	char name[4];  // the process name
	int wait = 0;  // counting waiting time
	int response_time = -1; // counting response time
	int priority; 

	if(argc < 4){
		printf("\nInsufficient Arguments..\n");
		return 1;
	}

	/* first argument : job time (second)
	second argument : delay time before execution (second)
	third argument : process name
	fourth argument : priority
	*/

	// Initialize the queue via syscall for each implementation
	if (syscall(ku_cpu_init_queue) != 0) {
		printf("\nFailed to initialize queue.\n");
        	return 1;
	} 
	jobTime = atoi(argv[1]);
	delayTime = atoi(argv[2]);
	strcpy(name,argv[3]);
	//priority = atoi(argv[4]); // for priority

	// wait for 'delayTime' seconds before execution
	sleep(delayTime);
	printf("\nProcess %s : I will use CPU by %ds. \n",name, jobTime);
	jobTime *= 10; // execute system call in every 0.1 second

	// continue requesting the system call as long as the jobTime remains
	while(jobTime){
		if (!syscall(339, name, jobTime)){
		//if (!syscall(340, name, jobTime, priority)){ // for priority(it has 3 parameters)
			jobTime--;
			// Record response time the first time the job gets CPU
			if (response_time == -1){ // duration of first waiting state
				response_time = (wait + 5) / 10;  // plus 5 for round-up				
			}
		}
		else wait++;  //if request is rejected, increase wait time
		usleep(100000); // delay 0.1 second		
	}

	syscall(339, name, jobTime); // for fcfs, srtf, rr
	//syscall(340, name, jobTime, priority); // for priority
	printf("\nProcess %s : Finish! My response time is %ds and My total wait time is %ds. ", name, response_time ,(wait + 5)/10);
	return 0;
}
