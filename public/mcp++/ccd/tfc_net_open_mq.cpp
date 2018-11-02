#include "tfc_net_open_mq.h"
#include "tfc_base_config_file.h"
#include "tfc_base_str.h"

using namespace tfc::net;
using namespace tfc::base;

CFifoSyncMQ* tfc::net::GetMQ(const string& conf_file)
{
	CFileConfig conf;
	conf.Init(conf_file);
	
	const int shm_key = from_str<int>(conf["root\\shm_key"]);
	const unsigned shm_size = from_str<unsigned>(conf["root\\shm_size"]);
	
	CShmMQ* shm_q = new CShmMQ();
	int ret = shm_q->init(shm_key, shm_size);
	assert(ret == 0);
	
	const unsigned sem_rlock = from_str<unsigned>(conf["root\\sem_rlock"]);
	const unsigned sem_wlock = from_str<unsigned>(conf["root\\sem_wlock"]);
	const string sem_name = conf["root\\sem_name"];
			
	CSemLockMQ* sem_q = new CSemLockMQ(*shm_q);
	ret = sem_q->init(sem_name.c_str(), sem_rlock, sem_wlock);
	assert(ret == 0);

	CFifoSyncMQ* fifo_q = new CFifoSyncMQ(*sem_q);
	const string fifo_path = conf["root\\fifo_path"];
	ret = fifo_q->init(fifo_path);

	assert(ret == 0);
	return fifo_q;
}
