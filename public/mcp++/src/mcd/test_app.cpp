
#include "tfc_cache_proc.h"
#include "iostream"

using namespace std;
using namespace tfc::cache;
using namespace tfc::net;

//////////////////////////////////////////////////////////////////////////

class CTestApp : public CacheProc
{
public:
	CTestApp(){}
	virtual ~CTestApp(){}
	virtual void run(const std::string& conf_file);
};

void CTestApp::run(const std::string& conf_file)
{
	cout<<__FILE__<<":"<<__LINE__<<"\t"<<"CTestApp::run"<<endl;
	
	cout<<"mq*"<<_mqs.size()<<":"<<endl;
	for(map<string, CFifoSyncMQ*>::iterator it = _mqs.begin()
		; it != _mqs.end()
		; it++)
	{
		//cout<<it->first<<"\t"<<(unsigned)it->second<<endl;
		cout<<it->first<<"\t"<<(size_t)it->second<<endl; // Sep 27th, uniqueruan.
	}
	
	cout<<"cache*"<<_caches.size()<<":"<<endl;
	for(map<string, CacheAccess*>::iterator it = _caches.begin()
		; it != _caches.end()
		; it++)
	{
		cout<<it->first<<"\t"<<(size_t)it->second<<endl; // Sep 27th, uniqueruan.
	}
	for(;;)
		pause();
}

extern "C"
{
	CacheProc* create_app()
	{
		return new CTestApp();
	}
}

//////////////////////////////////////////////////////////////////////////
///:~
