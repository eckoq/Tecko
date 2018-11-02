#include "tfc_cache_proc.h"
#include "tfc_net_open_mq.h"
#include "tfc_base_config_file.h"
#include "tfc_base_str.h"
#include "tfc_base_so.h"
#include "tfc_ipc_sv.hpp"
#include "version.h"
#include "mydaemon.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>

using namespace std;
using namespace tfc;
using namespace tfc::base;

int main(int argc, char* argv[]) {

	if (argc < 2) {
		printf("Usage:  %s so_file [-g]\n", argv[0]);
		printf("           -g     dlopen so_file with RTLD_GLOBAL flag\n");
		return -1;
	}

	if(!strncasecmp(argv[1], "-v", 2)) {
		printf("so_loader\n");
		printf("%s\n", version_string);
		printf("%s\n", compiling_date);
		return 0;
	}

    int dl_flag = RTLD_NOW;
	if(argc > 2 && !strncasecmp(argv[2], "-g", 2)) {
        dl_flag |=  RTLD_GLOBAL;
	}

	//
	// open dynamic lib
	//
    void* phdl = dlopen (argv[1], dl_flag);
    if (NULL == phdl)
    {
        printf("so_file open fail:\n%s\n", dlerror());
        return -1;
    }

	printf("so_file open ok!\n");

	return 0;
}
