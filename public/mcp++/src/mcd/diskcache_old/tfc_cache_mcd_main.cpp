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
#include <list>
#include <syslog.h>

using namespace std;
using namespace tfc;
using namespace tfc::ipc;
using namespace tfc::net;
using namespace tfc::base;
using namespace tfc::cache;
using namespace tfc::watchdog;

CWatchdogClient* wdc = NULL;		//watchdog client 
CWtgClientApi		*wtg = NULL;		// Wtg client.

int main(int argc, char* argv[]) {
	
	if (argc < 2) {
		printf("%s conf_file [non-daemon]\n", argv[0]);
		return 0;
	}
	
	if(!strncasecmp(argv[1], "-v", 2)) {
		printf("mcd\n");
		printf("%s\n", version_string);
		printf("%s\n", compiling_date);
		return 0;
	}

	if(argc == 2)
		mydaemon(argv[0]);
	else
		initenv(argv[0]);

	CFileConfig& page = * new CFileConfig();
	
	try {
		page.Init(argv[1]);
	}
	catch(...) {
		fprintf(stderr, "open config fail, %s, %m\n", argv[1]);
		exit(-1);
	}

	//
	// watchdog client
	//
	wtg = new CWtgClientApi;
	if ( !wtg ) {
		fprintf(stderr, "Wtg client alloc fail, %m\n");
		exit(-1);
	}

	try {
		string wdc_conf = page["root\\watchdog_conf_file"];
		wdc = new CWatchdogClient;
		if(wdc->Init(wdc_conf.c_str())) {
			fprintf(stderr, "watchdog client init fail, %m\n");
			exit(-1);
		}

		if ( wtg->Init(WTG_API_TYPE_MCP, 0, NULL, NULL)  ) {
			fprintf(stderr, "Wtg client init fail, %s,%m\n", wdc_conf.c_str());
			exit(-1);
		}

		wtg->ReportAlarm(1, "MCD restart!");
	}
	catch(...) {
		//watchdog 功能并不是必须的
	}

	//
	// open dynamic lib
	//	
	typedef CacheProc* (*app_constructor)();
	const string app_so_file = page["root\\app_so_file"];
	const string create_app_func = page["root\\create_app_func"];
	CSOFile so_file;
	int ret = so_file.open(app_so_file);	
	if(ret) {
		fprintf(stderr, "so_file open fail, %s, %m\n", app_so_file.c_str());
		exit(-1);
	}
	app_constructor constructor = (app_constructor) so_file.get_func(create_app_func);
	if(constructor == NULL) {
		fprintf(stderr, "so_file open func fail, %s, %m\n", app_so_file.c_str());
		exit(-1);
	}
	
	CacheProc* proc = constructor();
	
	//
	//	open mq
	//
	const map<string, string>& mqs = page.GetPairs("root\\mq");
	for(map<string, string>::const_iterator it = mqs.begin() ; it != mqs.end() ; it++) {
		CFifoSyncMQ* mq = GetMQ(it->second);
		assert(mq);
		proc->_mqs[it->first] = mq;
	}

	//
	//	open mem cache
	//
	const vector<string>& caches = page.GetSubPath("root\\cache");
	list< ptr<CShm> > shm_stub;
	for(vector<string>::const_iterator it = caches.begin() ; it != caches.end() ; it++) {
		const string cache_name = *it;
		int shm_key = from_str<int>(page["root\\cache\\" + cache_name + "\\shm_key"]);
		long shm_size = from_str<long>(page["root\\cache\\" + cache_name + "\\shm_size"]);
		unsigned node_total = from_str<unsigned>(page["root\\cache\\" + cache_name + "\\node_total"]);
		unsigned bucket_size = from_str<unsigned>(page["root\\cache\\" + cache_name + "\\bucket_size"]);
		unsigned chunk_total = from_str<unsigned>(page["root\\cache\\" + cache_name + "\\chunk_total"]);
		unsigned chunk_size = from_str<unsigned>(page["root\\cache\\" + cache_name + "\\chunk_size"]);

		bool bInited = true;
		ptr<CShm> shm;
		try {
			shm = CShm::create_only(shm_key, shm_size);
			memset(shm->memory(), 0, shm->size());
			bInited = true;
		}
		catch (ipc_ex& ex) {
			shm = CShm::open(shm_key, shm_size);
			bInited = false;
		}
		shm_stub.push_back(shm);

		CacheAccess* ca = new CacheAccess();
		ret = ca->open(shm->memory(), shm->size(), bInited, node_total, bucket_size, chunk_total, chunk_size);
		assert(ret == 0);
		
		proc->_caches[*it] = ca;
	}
	//	
	//	open disk cache
	//
	const vector<string>& disk_caches = page.GetSubPath("root\\disk_cache");
	for(vector<string>::const_iterator it = disk_caches.begin() ; it != disk_caches.end() ; it++) {
		const string cache_name = *it;
		int shm_key = from_str<int>(page["root\\disk_cache\\" + cache_name + "\\shm_key"]);
		long shm_size = from_str<long>(page["root\\disk_cache\\" + cache_name + "\\shm_size"]);
		unsigned node_total = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\node_total"]);
		unsigned bucket_size = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\bucket_size"]);
		unsigned chunk_total = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_total"]);
		unsigned chunk_size = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_size"]);

		string cache_file = page["root\\disk_cache\\" + cache_name + "\\cache_file"];
		bool bInited = true;
		ptr<CShm> shm;
		try {
			shm = CShm::create_only(shm_key, shm_size);
			memset(shm->memory(), 0, shm->size());
			bInited = true;
		}
		catch (ipc_ex& ex) {
			shm = CShm::open(shm_key, shm_size);
			bInited = false;
		}
		shm_stub.push_back(shm);

		diskcache::CacheAccess* ca = new diskcache::CacheAccess();
		ret = ca->open(shm->memory(), shm->size(), cache_file, bInited, node_total, bucket_size, chunk_total, chunk_size);
		assert(ret == 0);
		
		proc->_disk_caches[*it] = ca;
	}
	
	//初始化为监控mq使用的epoll对象
	if(proc->init_epoll_4_mq()) {
		fprintf(stderr, "init_epoll_4_mq fail, %m\n");
		exit(-1);
	}

	const string app_conf_file = page["root\\app_conf_file"];
	fprintf(stderr, "mcd started\n");
	proc->run(app_conf_file);
	fprintf(stderr, "mcd stopped\n");
	syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s mcd stopped\n", argv[0]);
	return 0;
}
