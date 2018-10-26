#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "tfc_base_fast_timer_str.h"

using namespace std;
using namespace tfc::base;

int main(int argc, char* argv[]) {
	char key[128];
	string skey;
	
	if(argc < 2) {
		printf("%s test_num\n", argv[0]);
		return -1;
	}
	int test_num = atoi(argv[1]);
	unsigned seq = 0;	
	struct timeval start, end;
	CFastTimerQueueStr queue;
	CFastTimerStrInfo* infos[test_num];
	CFastTimerStrInfo* info = NULL;
	int i;
	long t;
	int r;
	int get = 0, get_fail = 0;
	for(i = 0; i < test_num; ++i) {
		infos[i] = new CFastTimerStrInfo;
	}
	srand(time(0));

	gettimeofday(&end, NULL);	
	gettimeofday(&start, NULL);	
	for(i = 0; i < test_num; ++i) {
		r = rand();
		//queue.set(++seq, infos[i], 10000 + i);
		//queue.set(++seq, infos[i], 10000 + (rand() & 1023));
		++seq;
		memset(key, 0, 128);
		snprintf(key, 127, "key_%u", seq);
		skey = key;
		queue.set(skey, infos[i], 10000 - i);
		if((r % 6) > 0) {
			memset(key, 0, 128);
			snprintf(key, 128, "key_%u", r % seq);
			skey = key;
			queue.get(skey, &info);	
			if(info != NULL)
				get++;
			else
				get_fail++;	
		}
	}
	end.tv_sec -= 100;// ±º‰µπÕÀ100√Î
	for(i = 0; i < test_num; ++i) {
		queue.check_expire(end);	
	}

	gettimeofday(&end, NULL);
	t = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
	printf("test_num=%d, get=%d, get_fail=%d, t=%ld, count=%d\n", test_num, get, get_fail, t, queue.size());
	return 0;
}

