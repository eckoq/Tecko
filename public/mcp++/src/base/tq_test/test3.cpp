#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "AnsyTimerQueue.hpp"

int main(int argc, char* argv[]) {
	
	if(argc < 2) {
		printf("%s test_num\n", argv[0]);
		return -1;
	}
	int test_num = atoi(argv[1]);
	unsigned seq = 0;	
	struct timeval start, end;
	CAnsyTimerQueue queue(test_num);
	CTimerInfo* infos[test_num];
	CTimerInfo* info = NULL;
	int i;
	long t;
	int r;
	int get = 0, get_fail = 0;
	srand(time(0));
	for(i = 0; i < test_num; ++i) {
		infos[i] = new CTimerInfo;
		//infos[i]->SetAliveTimeOutMs(10000 + i);
		//infos[i]->SetAliveTimeOutMs(10000 + (rand() & 1023));
		infos[i]->SetAliveTimeOutMs(1000000 - i);
	}

	gettimeofday(&start, NULL);	
	for(i = 0; i < test_num; ++i) {
		queue.Set(++seq, infos[i]);
		r = rand();
		if((r % 6) > 0) {
			info = queue.Take(r % seq);
			if(info != NULL)
				get++;
			else
				get_fail++;	
		}
	}
	end.tv_sec -= 100;// ±º‰µπÕÀ100√Î
	for(i = 0; i < test_num; ++i) {
		queue.TimeTick(&end);	
	}

	gettimeofday(&end, NULL);
	t = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
	printf("test_num=%d, get=%d, get_fail=%d, t=%ld\n", test_num, get, get_fail, t);
	return 0;
}
