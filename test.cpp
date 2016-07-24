#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <sys/time.h>
#include <list>

#include "guard.h"
#include "distribute_lock_redis.h"

typedef struct thread_tag
{
	pthread_t pid;
}thread_t;


int nth=0;
int nloop =0;
int intr = 0;
int laten = 0;
DistrLockRedis* predis = NULL;

void* func(void*args)
{/*{{{*/
	int iter = nloop;
	while(iter--){
		pthread_t tid = pthread_self();
		//printf("%lu\t%s\n", tid, "run proc");
		//connection::ptr_t c = pool->get(connection::MASTER);
		{
			char buf[32] = {'0'};
			sprintf(buf,"footest_%d", iter);
			std::string key = buf;
			printf("lockkey:%s\n", key.c_str());
			//
			Guard<DistrLockRedis::ptr, std::string, int> guard(predis, key, 10);
			//Lock lock(c, key.c_str(), "bartest", 60);
			//Guard<Lock::ptr> guard(&lock);
			//
			std::cout <<"get:" << predis->Get(key) << std::endl;
			if(laten != 0){
				usleep(1000*1000*laten);
			}
			if(intr != 0){
				exit(-1);
			}
		}
		//pool->put(c);
	}
	return NULL;
}/*}}}*/

int main(int argc, char** argv)
{/*{{{*/
	pid_t pid = getpid();
	//printf("%d\t%s\n", int(pid), "run main");

	nth = atoi(argv[1]);
	nloop  = atoi(argv[2]);
	intr = atoi(argv[3]);
	laten= atoi(argv[4]);
	int i=0;
	predis = new DistrLockRedis();
	predis->Initialize("localhost", "mymaster", 26379);

	thread_t *thread_pool = (thread_t*)calloc(nth, sizeof(thread_t));
	if (NULL == thread_pool) {
		exit(-1);
	}

	time_t start = time(NULL);
	for (i=0;i<nth;i++) {
		pthread_create(&thread_pool[i].pid, NULL, &func, NULL);
	}
	for (i=0;i<nth;i++) {
		pthread_join(thread_pool[i].pid, NULL);
	}

	time_t end = time(NULL);
	printf("thread:%d, loop:%d, time:%f\n", nth, nloop, difftime(end, start));

	free(thread_pool);

	return EXIT_SUCCESS;
}/*}}}*/
