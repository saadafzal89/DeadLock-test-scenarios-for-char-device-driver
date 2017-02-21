#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include<sys/types.h>

#define CDRV_IOC_MAGIC 'Z'
#define E2_IOCMODE1 _IOWR(CDRV_IOC_MAGIC, 1, int)
#define E2_IOCMODE2 _IOWR(CDRV_IOC_MAGIC, 2, int)

//sem_t sem;
char read_buf[200];
char write_buff[200]="This is a test statement";

pthread_mutex_t global_lock1, global_lock2;

void *testing (void *threadid)
{
	long tid;
	tid = (long)threadid;
	int fp, inp, rc;
	fp = open("/dev/a5", O_RDWR);
	if(fp == -1) 
	{
		printf("File either does not exist or has been locked by another process\n");
		exit(-1);
	}
	
	if(tid==0)
	{
		printf("Thread[%ld] calling IOCTL to change to mode2\n",tid);
		rc = ioctl(fp, E2_IOCMODE2, 0);
		if (rc == -1)
		{ 
			perror("\nError calling IOCTL\n");
			pthread_exit(NULL);
		}	
	}

	else if(tid==1)
	{
		printf("Thread[%ld] trying to get global lock2\n",tid);
		pthread_mutex_lock (&global_lock2);
		printf("Thread[%ld] has global lock2\n",tid);
		printf("Thread[%ld] is now writing\n",tid);
		write(fp,write_buff,sizeof(write_buff));
		usleep(500000);
		printf("Thread[%ld] trying to get global lock1\n",tid);
		pthread_mutex_lock (&global_lock1);
		printf("Thread[%ld] has global lock1\n",tid);
		printf("Thread[%ld] is now reading\n",tid);
		read(fp,read_buf,sizeof(read_buf));
		pthread_mutex_unlock (&global_lock1);
		printf("Thread[%ld] has released global lock1\n",tid);
		pthread_mutex_unlock (&global_lock2);
		printf("Thread[%ld] has released global lock2\n",tid);
		
	}
	
	else 
	{
		//usleep(500000);
		printf("Thread[%ld] trying to get global lock1\n",tid);
		pthread_mutex_lock (&global_lock1);
		printf("Thread[%ld] has global lock1\n",tid);
		printf("Thread[%ld] is now reading\n",tid);
		read(fp,read_buf,sizeof(read_buf));
		usleep(500000);
		printf("Thread[%ld] trying to get global lock2\n",tid);
		pthread_mutex_lock (&global_lock2);
		printf("Thread[%ld] has global lock2\n",tid);
		printf("Thread[%ld] is now writing\n",tid);
		write(fp,write_buff, sizeof(write_buff));
		pthread_mutex_unlock (&global_lock2);
		printf("Thread[%ld] has released global lock2\n",tid);
		pthread_mutex_unlock (&global_lock1);
		printf("Thread[%ld] has released global lock1\n",tid);
		
	}
	
	close(fp);
	printf("Exiting thread[%ld]\n",tid);
	pthread_exit(NULL);
}


int main (int argc, char *argv[])
{
	int num_of_threads, sem_ret, thr, first;
	long i,k;
	num_of_threads =2;
	//num_of_threads = atoi(argv[1]);	
	pthread_t first_thread;
	pthread_t threads[num_of_threads];
	//sem_ret = sema_init(&sem,0,1);
	

	//creating thread for change to mode1
	printf("Creating thread[0]\n");
	first = pthread_create(&first_thread, NULL, testing, 0);
	if(first)
	{
		printf("ERROR; return code from pthread_create() is %d\n", first);
         exit(-1);
	}

	usleep(200000);  //sleeping so that above thread first changes the mode to Mode1 

	//creating the pthreads
	for (i =1;i<=num_of_threads;i++)
	{
		printf("Creating thread[%ld]\n",i);
		thr= pthread_create(&threads[i], NULL, testing, (void*)i);
		if(thr)
		{
			printf("ERROR; return code from pthread_create() is %d\n", thr);
         	exit(-1);
		}
	}

	//joining the threads

	pthread_join(first_thread, NULL); //first thread	
	
	for (k =1; k<=num_of_threads; k++) //rest of threads
	{
		pthread_join(threads[k], NULL);
	}

	pthread_mutex_destroy(&global_lock1);
	pthread_mutex_destroy(&global_lock2);
	return(0);
}
