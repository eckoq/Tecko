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
#ifdef _SHMMEM_ALLOC_
#include "tfc_net_open_shmalloc.h"
#include "myalloc.h"
#endif
using namespace std;
using namespace tfc;
using namespace tfc::ipc;
using namespace tfc::net;
using namespace tfc::base;
using namespace tfc::cache;
using namespace tfc::watchdog;

CWatchdogClient* 	wdc = NULL;		//watchdog client 
CWtgClientApi*		wtg = NULL;		// Wtg client.

#ifdef _ENABLE_TNS_
#include "tns_nagent_api.h"
using namespace storage::tns::agent_api;
class CTnsAgentApi		*tns_api = NULL;		// TNS Client.
#endif

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

	// Bind CPU.
	try {
		int bind_cpu = from_str<int>(page["root\\bind_cpu"]);
		cpubind(argv[0], bind_cpu);
	}
	catch (...) {
	}

#ifdef _ENABLE_TNS_
	// For tns.
	try {
		string tns_conf = page["root\\tns_conf_file"];
		tns_api = new CTnsAgentApi;
		if ( tns_api->init(tns_conf) ) {
			fprintf(stderr, "TNS client init fail!\n");
			exit(-1);
		}
	} catch (...) {
		fprintf(stderr, "Tns not launched for no config!\n");
		tns_api = NULL;
	}
#endif

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
	// watchdog client
	//
	try {
		wtg = new CWtgClientApi;
	} catch (...) {
		fprintf(stderr, "Wtg client alloc fail, %m\n");
		exit(-1);
	}

	try {
		string wdc_conf = page["root\\watchdog_conf_file"];
		try {
			wdc = new CWatchdogClient;
		} catch (...) {
			fprintf(stderr, "Out of memory for watchdog client!\n");
			exit(-1);
		}

		// Get frame version.
		char *frame_version = (strlen(version_string) > 0 ? version_string : NULL);

		// Get plugin version.
		const char *plugin_version = NULL;
		get_plugin_version pv_func = (get_plugin_version)so_file.get_func("get_plugin_version_func");
		if ( pv_func ) {
			plugin_version = pv_func();
		} else {
			plugin_version = NULL;
		}

		// Get addition 0.
		const char *add_0 = NULL;
		get_addinfo_0 add0_func = (get_addinfo_0)so_file.get_func("get_addinfo_0_func");
		if ( add0_func ) {
			add_0 = add0_func();
		} else {
			add_0 = NULL;
		}

		// Get addition 1.
		const char *add_1 = NULL;
		get_addinfo_1 add1_func = (get_addinfo_1)so_file.get_func("get_addinfo_1_func");
		if ( add1_func ) {
			add_1 = add1_func();
		} else {
			add_1 = NULL;
		}
		
		if( wdc->Init(wdc_conf.c_str(), PROC_TYPE_MCD, frame_version, plugin_version,
			NULL, add_0, add_1) ) {
			fprintf(stderr, "watchdog client init fail, %s,%m\n", wdc_conf.c_str());
			exit(-1);
		}

		if ( wtg->Init(WTG_API_TYPE_MCP, 0, NULL, NULL)  ) {
			fprintf(stderr, "Wtg client init fail, %s,%m\n", wdc_conf.c_str());
			exit(-1);
		}
	}
	catch(...) {
		//watchdog 功能并不是必须的
	}
	
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
#if 0		
		//unsigned chunk_total = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_total"]);
		//unsigned chunk_size = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_size"]);
#else	
		unsigned long long filesize;
		unsigned minchunksize;
		try {
			filesize = from_str<unsigned long long>(page["root\\disk_cache\\" + cache_name + "\\filesize"]);
			minchunksize = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\minchunksize"]);
		}
		catch(...) {
			unsigned chunk_total = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_total"]);
			unsigned chunk_size = from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_size"]);
			filesize = (unsigned long long)chunk_total * (unsigned long long)chunk_size;
			minchunksize = chunk_size >> 1;
		}
#endif
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
		//ret = ca->open(shm->memory(), shm->size(), cache_file, bInited, node_total, bucket_size, chunk_total, chunk_size);
		ret = ca->open(shm->memory(), shm->size(), cache_file, bInited, node_total, bucket_size, filesize, minchunksize);
		assert(ret == 0);
		
		proc->_disk_caches[*it] = ca;
	}

#ifdef _SHMMEM_ALLOC_
	//
	// share memory allocator
	//
	try {
		if(OpenShmAlloc(page["root\\shmalloc\\shmalloc_conf_file"])) {
			fprintf(stderr, "shmalloc init fail, %m\n");
			exit(-1);
		}
		else {
			fprintf(stderr, "mcd shmalloc enable\n");
		}
	}
	catch(...) {
		fprintf(stderr, "mcd shmalloc disable\n");
	}
#endif

	//初始化为监控mq使用的epoll对象
	if(proc->init_epoll_4_mq()) {
		fprintf(stderr, "init_epoll_4_mq fail, %m\n");
		exit(-1);
	}

	const string app_conf_file = page["root\\app_conf_file"];
	fprintf(stderr, "mcd started\n");
	proc->run(app_conf_file);
	
#ifdef _SHMMEM_ALLOC_
	myalloc_fini();
#endif
	fprintf(stderr, "mcd stopped\n");
	syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s mcd stopped\n", argv[0]);
	return 0;
}
