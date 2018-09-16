/*#######################################################################################################################################################
 CSE522 Real-time Embedded Systems – Assignment 1 by Group 2 (Hena Shah ( ASU ID: 1213348511) & Nidhi Dubey ( ASU ID: 1213031246))

In this code we have programmed real-time tasks on Linux environment, including periodic and aporadic tasks, event handling, priority inheritance.
As per the instructions given in the assignment guidelines pdf, the input file will follow the below pattern:

<task_body> ::= <compute> { < CS> <compute> }
<CS> ::= <lock_m> <compute> {<CS> <compute>} <unlock_m >

which means that there will be always a mutex lock/unlock between two consecutive computations. This pattern is also followed by the sample input file given in the pdf.
So following this pattern, we have interpreted our code in a way that computations and lock/unlock will always be alternate ( computations at even postions and mutex at odd positions in our task body).
We have also attached a sample input file which is in accordance with the guidelines and input file in the question. You may also use your input file which follows the same pattern.Change the mouse event ( At line 29) and the input file name ( At line 320) if required. Change task size if reuired (at line 28).

######################################################################################################################################################*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <semaphore.h>
#define SIZE 20 	// Size for creating taskbody. You may change this as per your input
#define MOUSEFILE "/dev/input/event7"       // Mouse event number. Change this as per your mouse event number.

//valgrind --tool=memcheck -v ./main

int i=0,j=0,k=0,z,q=0;
int READY;	// READY FLAG to activate all threads at the same time             					                      
int choice;	// Choice to run either PI enable or PI diabled code
int fd;		
int STOP_FLAG=0;	//Stop flag to determine timeout

pthread_cond_t event_0,event_1,all_ready;

pthread_mutex_t Lock[10];					//PI disabled mutex locks    
pthread_mutexattr_t Pi_Lock[10];				//PI enabled mutex lock attribute
pthread_mutex_t act_wait = PTHREAD_MUTEX_INITIALIZER;		//mutex lock for activating all threads together
pthread_mutex_t event0_wait = PTHREAD_MUTEX_INITIALIZER;	//mutex lock for event0 -> left event
pthread_mutex_t event1_wait = PTHREAD_MUTEX_INITIALIZER;	//mutex lock for event1 -> right event

//arguments structure for periodic threads
struct PeriodicParameters		
{
	int tasklen;
    	int period;
	int taskbody[SIZE];
};

//arguments structure for aperiodic threads
struct AperiodicParameters		
{
	int tasklen;
    	int click;
	int taskbody[SIZE];
};

// Empty structure pointers to reset pointer after every loop
static struct PeriodicParameters* EmptyStruct;  
static struct AperiodicParameters* EmptyStruct2;

// Computation loop
void computation(int iter)
{
	int u,v=0;
	for(u=0;u<iter;u++)
	{
	v=v+u;
	}
}

// Periodic thread function task body
//#############################################################################################################
void* PeriodicFunction(void* parameters)
{	
	// wait till all threads are ready for activation
	pthread_mutex_lock(&act_wait);
    	pthread_cond_wait(&all_ready,&act_wait);
   	pthread_mutex_unlock(&act_wait);

	struct timespec deadline;	//stuct timespec variable
    	clock_gettime(CLOCK_MONOTONIC, &deadline);
	
	while(STOP_FLAG==0)	// run until timeout
	{
	struct PeriodicParameters* p = (struct PeriodicParameters*) parameters;
	printf("\n\n Now executing for Periodic thread with period :  %d",p->period);


	// Checking for computations and locks
	for(i=0;i<p->tasklen;i++)
	{
    		if(i%2==0)
		{
			printf("\n Calling Computation for Periodic thread for iter %d",p->taskbody[i]);
			computation(p->taskbody[i]);
		}

		/*As we have replaced L and U by 8 and 9 in our task body so now L8 and U8 will be 88 and 98 respectively. Now we are dividing this by 80 or 90 and remainder will determine which
		Lock will be used. We have also checked the position so that this doesn't conflict with the case where computations are for 88 or 98 times. */ 
		else 
		{      
			if((p->taskbody[i]%80)==10)
			{
				 pthread_mutex_init(&Lock[0],NULL);
                    		 pthread_mutex_lock(&Lock[0]);
                   		 printf("\n Locking mutex for Periodic thread: %d",(p->taskbody[i]%80));
				
			}
			if(p->taskbody[i]%80<10)
			{
				 pthread_mutex_init(&Lock[p->taskbody[i]%80],NULL);
                    		 pthread_mutex_lock(&Lock[p->taskbody[i]%80]);
                   		 printf("\n Locking mutex for Periodic thread: %d",(p->taskbody[i]%80));
			}
			
			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[0]);
                   		 printf("\n Unlocking mutex for Periodic thread: %d",(p->taskbody[i]%90));
			}

			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[p->taskbody[i]%90]);
                   		 printf("\n Unlocking mutex for Periodic thread: %d",(p->taskbody[i]%90));
			}
			
			
		}

	}

	deadline.tv_nsec += p->period*1000000;
      	if(deadline.tv_nsec >= 1000000000)
      	{
		deadline.tv_nsec -= 1000000000;
		deadline.tv_sec++;
	}
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, 0);	

	p = EmptyStruct;	//reset the pointer for next loop
	}

        printf("\n Periodic Task exiting..."); // if timeoutx exit loop
        pthread_exit(NULL);
	return 0;
}

// Aperiodic thread function task body
//#############################################################################################################
void* AperiodicFunction(void* parameters)
{
	// wait for all threads to be ready for activation
	pthread_mutex_lock(&act_wait);
    	pthread_cond_wait(&all_ready,&act_wait);
   	pthread_mutex_unlock(&act_wait);
	
	
	while(STOP_FLAG==0)
	{
	struct AperiodicParameters* p = (struct AperiodicParameters*) parameters;
	if(p->click==0) // check if it's for event 0 or event 1
	{	
		// wait till left click for event 0
		pthread_mutex_lock(&event0_wait);
            	pthread_cond_wait(&event_0,&event0_wait);
           	pthread_mutex_unlock(&event0_wait);
		
		// even if thread is waiting it should exit if it's timeout. For this we are broadcasting from main after timeout.
           	if(STOP_FLAG==1)
               	break;

		printf("\n Now executing for Aperiodic thread with event : %d",p->click);
		for(i=0;i<p->tasklen;i++)
		{
		/*As we have replaced L and U by 8 and 9 in our task body so now L8 and U8 will be 88 and 98 respectively. Now we are dividing this by 80 or 90 and remainder will determine which
		Lock will be used. We have also checked the position so that this doesn't conflict with the case where computations are for 88 or 98 times. */ 
    		if(i%2==0)
		{
			printf("\n Calling Computation for Aperiodic thread for iter %d",p->taskbody[i]);
			computation(p->taskbody[i]);
		}
		else 
		{      
			if((p->taskbody[i]%80)==10)
			{
				 pthread_mutex_init(&Lock[0],NULL);
                    		 pthread_mutex_lock(&Lock[0]);
                   		 printf("\n Locking mutex for Aperiodic thread: %d",(p->taskbody[i]%80));
				
			}
			if(p->taskbody[i]%80<10)
			{
				 pthread_mutex_init(&Lock[p->taskbody[i]%80],NULL);
                    		 pthread_mutex_lock(&Lock[p->taskbody[i]%80]);
                   		 printf("\n Locking mutex for Aperiodic thread:  %d",(p->taskbody[i]%80));
			}
			
			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[0]);
                   		 printf("\n Unlocking mutex for Aperiodic thread:  %d",(p->taskbody[i]%90));
			}

			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[p->taskbody[i]%90]);
                   		 printf("\n Unlocking mutex for Aperiodic thread:  %d",(p->taskbody[i]%90));
			}		
		}
		}
		}

	if(p->click==1) // for right click event 1
	{		
			// wait till left click for event 0
			pthread_mutex_lock(&event1_wait);
            		pthread_cond_wait(&event_1,&event1_wait);
           		pthread_mutex_unlock(&event1_wait);

			/*As we have replaced L and U by 8 and 9 in our task body so now L8 and U8 will be 88 and 98 respectively. Now we are dividing this by 80 or 90 and remainder will determine which
			Lock will be used. We have also checked the position so that this doesn't conflict with the case where computations are for 88 or 98 times. */ 
           		if(STOP_FLAG==1)
               	        break;

			printf("\n Now executing for Aperiodic thread with event : %d",p->click);
			for(i=0;i<p->tasklen;i++)
			{
	    		if(i%2==0)
			{
				printf("\n Calling Computation for Aperiodic thread for iter %d",p->taskbody[i]);
				computation(p->taskbody[i]);
			}
			else 
			{      
			if((p->taskbody[i]%80)==10)
			{
				 pthread_mutex_init(&Lock[0],NULL);
                    		 pthread_mutex_lock(&Lock[0]);
                   		 printf("\n Locking mutex for Aperiodic thread: %d",(p->taskbody[i]%80));
				
			}
			if(p->taskbody[i]%80<10)
			{
				 pthread_mutex_init(&Lock[p->taskbody[i]%80],NULL);
                    		 pthread_mutex_lock(&Lock[p->taskbody[i]%80]);
                   		 printf("\n Locking mutex for Aperiodic thread:  %d",(p->taskbody[i]%80));
			}
			
			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[0]);
                   		 printf("\n Unlocking mutex for Aperiodic thread:  %d",(p->taskbody[i]%90));
			}

			else if(p->taskbody[i]%90<10)
			{
                    		 pthread_mutex_unlock(&Lock[p->taskbody[i]%90]);
                   		 printf("\n Unlocking mutex for Aperiodic thread:  %d",(p->taskbody[i]%90));
			}	
		}
		}
		}
	p = EmptyStruct2; //reset pointer

	}			
	// exit at timeout	
	printf("\n Aperiodic Task exiting...");

	pthread_exit(NULL);
	return 0;
	}

// Mouse event thread task body
//#############################################################################################################
void* Mouse_Function(void* parameters)
{
	
		printf("\n Inside Mouse Thread");
		
		struct input_event ie;
		fd = open(MOUSEFILE, 0);

		if(fd == -1)
		{
			perror("opening device");
			exit(EXIT_FAILURE);
		}
		// Check for specific mouse events
		 while(STOP_FLAG==0)
		{
			read(fd, &ie, sizeof(struct input_event));
			if(ie.code == 272 && ie.value == 1 )
			{
				printf("\n\n Left key pressed");
				 pthread_cond_broadcast(&event_0);
			}


			else if(ie.code == 273 && ie.value == 1 )
			{
				printf("\n\n Right key pressed");
				pthread_cond_broadcast(&event_1);
			}
		}
		return 0;
}

//#############################################################################################################
int main()
	{ 
   	int taskNumber,len;
   	char c[10],task[0];
   	i=0;
   	int timeout;
	int count = 0;
	FILE *file = fopen("test.txt", "r");   // to open input file. Change if required. 
	int PeriodicCount=0, AperiodicCount=0;
  	char line[1024];

   	fgets(line, sizeof(line), file); /* read a line */
     	task[0]=line[i];
    	taskNumber = atoi(task); // compute number of tasks

	int array[taskNumber][SIZE];
	int prio[taskNumber];
	int pri_click[taskNumber];
	int task_length[taskNumber];
	char taskorder[taskNumber];
	struct timespec stop;

   	printf("\n Number of tasks: %d ",taskNumber);
     	i=i+2;
     	j=0;k=0;q=0;
	//to compute timeout
	while(line[i]!='\0')
  	{
		if(line[i]!=' ')
		{
    			c[j]=line[i];
			//printf("\n3 read: %c\n", c[j]);
			i++;
			j++;
		}
	}
	timeout = atoi(c);
	printf("\n Timeout: %d \n", timeout);
	i=0;
	j=0;
	c[0]='\0';// empty string

	printf("\n Select your option: 1. PI enabled 2. PI disabled \n");
	scanf("%d",&choice);

	// to read each line separately
	while(count!=taskNumber)
	{
		q=0;
		len=0;
	 	fgets(line, sizeof line, file); //read a line

		int length=strlen(line);
		//printf("len: %d \n",length);

		for( i=0;i<length;i++)
		{
			if(line[i]=='L') // lock =8
			{
				line[i]='8';
			}
			if(line[i]=='U') // Unlock =9
			{
				line[i]='9';
			}

		}
		//printf("\n %s",line);
		i=0;
		j=0;
		c[j]='\0';// empty string

		//count number of periodic threads
		if(line[i]=='P')
		{
			taskorder[k]='P';
			PeriodicCount++;
		}

		//count number of aperiodic threads
		else if(line[i]=='A')
		{
			taskorder[k]='A';
			AperiodicCount++;
		}
		i=i+2;

		// to find priority
		while(line[i]!=' ')
		{
    			c[j]=line[i];
			//printf("\n3 read: %c\n", c[j]);
			i++;
			j++;
		}
		c[j]='\0';
		prio[k] = atoi(c);

		i=i+1;
		j=0;


                // to find task period or event number
		while(line[i]!=' ')
		{
    			c[j]=line[i];
			//printf("\n3 read: %c\n", c[j]);
			i++;
			j++;
		}
		c[j]='\0';
	        pri_click[k] = atoi(c);

	 	i++;
		j=0;

		// build task body
		while(line[i]!='\0')
		{
			j=0;
			c[0]='\0';
			while(line[i]!=' ')
			{
				c[j]=line[i];
				j++;
				i++;
				if(line[i]=='\n')
				{
					break;
				}
			}
			c[j]='\0';
			i++;
			array[k][q]=atoi(c);
			//printf("array value %d %d :  %d \n",k,q,array[k][q]);
			len++;
			q++;
		}
		task_length[k]=len;
		k++;
		count++;
	}
	// To set affinity so all threads run on same core	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	
	// Create thread ID's	
	pthread_t periodic_id[PeriodicCount];
	pthread_t aperiodic_id[AperiodicCount];
	pthread_t mouseclick;

	int t1=0;
	int t2=0;

	pthread_attr_t ptattr;
	pthread_attr_t atattr;
	pthread_attr_t mattr;

	struct AperiodicParameters *y;
	struct PeriodicParameters *z;

	for(k=0;k<taskNumber;k++)
	{
		// For creating Periodic threads
		if(taskorder[k]=='P')
		{
			//set thread sched policy as FIFO and priority
    			struct sched_param param;				//create struct sched_param variable
			pthread_attr_init(&ptattr);
       			pthread_attr_setinheritsched(&ptattr,PTHREAD_EXPLICIT_SCHED);
       			pthread_attr_setschedpolicy(&ptattr, SCHED_FIFO);
       			param.sched_priority = prio[k];
       			pthread_attr_setschedparam(&ptattr,&param); 
    			z = (struct PeriodicParameters*)malloc(sizeof(struct PeriodicParameters));
    			z->period = pri_click[k];
		
			z->tasklen=task_length[k];
			for(i=0;i<task_length[k];i++)     
			{
    			z->taskbody[i] = array[k][i];
			}
			
			pthread_create (&periodic_id[t1], &ptattr, &PeriodicFunction, z);	//create periodic threads
			pthread_setaffinity_np(periodic_id[t1],sizeof(cpu_set_t),&cpuset);	// set affinity

			t1++;
		}

		else if(taskorder[k]=='A')
		{
			// set thread sched policy as FIFO and priority
    			struct sched_param aparam;				//create struct sched_param variable

			pthread_attr_init(&atattr);
       			pthread_attr_setinheritsched(&atattr,PTHREAD_EXPLICIT_SCHED);
        		pthread_attr_setschedpolicy(&atattr, SCHED_FIFO);
        		aparam.sched_priority = prio[k];
        		pthread_attr_setschedparam(&atattr,&aparam); 

			y = (struct AperiodicParameters*)malloc(sizeof(struct AperiodicParameters));
    			y->click = pri_click[k];
			y->tasklen=task_length[k];
			for(i=0;i<task_length[k];i++)    
			{
    			y->taskbody[i] = array[k][i];
			}

			pthread_create(&aperiodic_id[t2], &atattr, &AperiodicFunction, y);	//create aperiodic threads 
			pthread_setaffinity_np(aperiodic_id[t2],sizeof(cpu_set_t),&cpuset);	//set affinity
			
			t2++;
		}
		}
			

	printf("\n Completion of thread creation. All threads are ready! \n");
	READY=1;	// after all threads are created, set ready flag as 1
	
	// set mouse thread sched policy as FIFO and priority as 99
	struct sched_param mparam;			
	pthread_attr_init(&mattr);
	pthread_attr_setinheritsched(&mattr,PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&mattr, SCHED_FIFO);
	mparam.sched_priority =99;
	pthread_attr_setschedparam(&mattr,&mparam); 

	pthread_create (&mouseclick, NULL, &Mouse_Function,NULL);	//create mouse thread
	
	// Mutex intialisations for PI disabled 
	if(choice==2)
	{
	for(i=0;i<10;i++)
	{
		pthread_mutex_init(&Lock[i],NULL);
	}
	}

	// Mutex intialisations for PI enabled 
	if(choice==1)
	{
	for(i=0;i<10;i++)
	{
		 pthread_mutexattr_init(&Pi_Lock[i]);
                 pthread_mutexattr_setprotocol(&Pi_Lock[i],PTHREAD_PRIO_INHERIT);
                 pthread_mutex_init(&Lock[i],&Pi_Lock[i]);
	}
	}

	
	pthread_cond_broadcast(&all_ready);

	// keep checking for timeout
	clock_gettime(CLOCK_MONOTONIC,&stop);
	stop.tv_sec += timeout/1000;
	stop.tv_nsec += (timeout%1000)*1000000ul;
	if(stop.tv_nsec > 1000000000ul)
	{
    		stop.tv_nsec = 0;
    		stop.tv_sec++;
	}

	clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&stop,0);

	// if timeout set stop flag=1 and broadcast waiting threads
	STOP_FLAG=1;
	pthread_cond_broadcast(&event_1);
	pthread_cond_broadcast(&event_0);
	usleep(1000);

	// Destroy all conditional waiting events
	pthread_cond_destroy(&all_ready);
	pthread_cond_destroy(&event_0);
	pthread_cond_destroy(&event_1);

	// Join all threads
	for (i = 0; i <PeriodicCount ; i++)
		{
        	pthread_join(periodic_id[i],NULL);
		}

 	for (i = 0; i <AperiodicCount ; i++)
		{
        	pthread_join(aperiodic_id[i],NULL);
		}

	// Destroy all mutex locks
	if(choice==2)
	{
	for(i=0;i<10;i++)
	{
		pthread_mutex_destroy(&Lock[i]);
	}
	}

	if(choice==1)
	{
	for(i=0;i<10;i++)
	{
		  pthread_mutexattr_destroy(&Pi_Lock[i]);
                  pthread_mutex_destroy(&Lock[i]);
	
	}
	}

	pthread_mutex_destroy(&act_wait);
	pthread_mutex_destroy(&event0_wait);
	pthread_mutex_destroy(&event1_wait);
	
	fclose(file);	// close input file
	printf("\n PROGRAM TERMINATED\n");
	return 0;
	}

// End of program.
