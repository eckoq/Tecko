#ifndef _TFC_CACHE_PROC_H_
#define _TFC_CACHE_PROC_H_
#include <map>
#include <string>
#include "tfc_net_ipc_mq.h"
#include "tfc_cache_access.h"
#include "tfc_diskcache_access.h"
#include "tfc_base_watchdog_client.h"

#include "wtg_client_api.h"

//////////////////////////////////////////////////////////////////////////
using namespace tfc::net;
using namespace tfc::watchdog;
using namespace tfc::wtgapi;

extern bool stop;
extern CWatchdogClient* wdc;
extern CWtgClientApi		*wtg;

#ifdef _ENABLE_TNS_
#include "tns_nagent_api.h"
using namespace storage::tns::agent_api;
extern class CTnsAgentApi		*tns_api;		// TNS Client.
#endif
#define MAX_MQ_NUM		(1<<6)
namespace tfc{namespace cache
{
	// Just return string information.
	typedef const char* (*get_plugin_version)();
	typedef const char* (*get_addinfo_0)();
	typedef const char* (*get_addinfo_1)();
	//��mq��epoll����ʱ�����ø����͵Ļص�����������mq��Ϣ
	typedef void (*disp_func)(void*);
	typedef struct {
		CFifoSyncMQ* _mq;		//����mq
		disp_func	_func;		//��������mq��epoll�����ʱ����õĻص�����
		void* _priv;			//�ص��������Զ������
		bool _active;			//�Ƿ�epoll����
	}MQInfo;

	class CacheProc
	{
	public:
		CacheProc(): _epfd(-1), _infonum(0){}
		virtual ~CacheProc(){ close(_epfd);}
		virtual void run(const std::string& conf_file) = 0;
		std::map<std::string, tfc::net::CFifoSyncMQ*> _mqs;
		std::map<std::string, tfc::cache::CacheAccess*> _caches;
		std::map<std::string, tfc::diskcache::CacheAccess*> _disk_caches;
		
		//��ʼ��mq��epoll��ʩ	
		int init_epoll_4_mq();
		//�ڶ�̬���runѭ���е��ô˺�����������mq�Ĵ�����
		int run_epoll_4_mq();
		//func�Ǵ���mq�¼��Ļص�������һ����CacheProc����ĳ�Ա������privһ����CacheProc����Ķ���ָ��
		int add_mq_2_epoll(CFifoSyncMQ* mq, disp_func func, void* priv);
	protected:
		int _epfd;
		MQInfo	_mq_info[MAX_MQ_NUM];
		int _infonum;
	};
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_CACHE_PROC_H_
///:~

