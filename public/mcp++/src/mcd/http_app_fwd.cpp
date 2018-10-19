#include "tfc_cache_proc.h"
#include "tfc_net_ccd_define.h"
#include "tfc_net_dcc_define.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef _SHMMEM_ALLOC_
#include "myalloc.h"
#endif

using namespace tfc::cache;
using namespace tfc::net;

void disp_ccd(void* priv);
void disp_dcc(void* priv);

class CHttpApp : public CacheProc
{
public:
	CHttpApp(){}
	virtual ~CHttpApp(){}
	virtual void run(const std::string& conf_file);

	void dispatch_ccd();
	void dispatch_dcc();
		
	CFifoSyncMQ* mq_ccd_2_mcd;
	CFifoSyncMQ* mq_mcd_2_ccd;
	CFifoSyncMQ* mq_mcd_2_dcc;
	CFifoSyncMQ* mq_dcc_2_mcd;
	
	char buf[1<<24];
	TCCDHeader* ccdheader;
	TDCCHeader* dccheader;
};

void CHttpApp::run(const std::string& conf_file)
{
	mq_ccd_2_mcd = _mqs["mq_ccd_2_mcd"];
	mq_mcd_2_ccd = _mqs["mq_mcd_2_ccd"];
	mq_mcd_2_dcc = _mqs["mq_mcd_2_dcc"];
	mq_dcc_2_mcd = _mqs["mq_dcc_2_mcd"];

	//由于TCCDHeader和TDCCHeader的长度相同，所以这里把头部指向了同一个缓冲区
	ccdheader = (TCCDHeader*)buf;
	dccheader = (TDCCHeader*)buf;	
	
	//添加mq_ccd_2_mcd到epoll监控
	add_mq_2_epoll(mq_ccd_2_mcd, disp_ccd, this);
	//添加mq_dcc_2_mcd到epoll监控
	add_mq_2_epoll(mq_dcc_2_mcd, disp_dcc, this);

	fprintf(stderr, "mcd so started\n");
	//主循环
	while(!stop) {
		
		//监控mq事件并运行相应的回调函数
		run_epoll_4_mq();	
		
		//其他事务处理
		//...
	}

	fprintf(stderr, "mcd so stopped\n");
}
void CHttpApp::dispatch_ccd()
{
	unsigned data_len;
	unsigned flow;
	int ret;
	
	do
	{
		data_len = 0;
		//这里使用try_dequeue，永远不会阻塞
		ret = mq_ccd_2_mcd->try_dequeue(buf, 1<<24, data_len, flow);
		if(!ret && data_len > 0) {
			if(ccdheader->_type == ccd_rsp_data || ccdheader->_type == ccd_rsp_data_shm) {

				//为测试方便，硬编码了后台服务器ip和port
				dccheader->_ip = (unsigned)inet_addr("10.1.252.12");
				dccheader->_port = 20000;
				//设置传输数据的方式
#ifdef _SHMMEM_ALLOC_
				if(ccdheader->_type == ccd_rsp_data_shm) {
					dccheader->_type = dcc_req_send_shm;
					printf("recv from ccd shmalloc data\n");
				}	
				else				
#endif
					dccheader->_type = dcc_req_send; 

				//这里直接把ccd发过来的请求原封不动转发到dcc去，再由dcc去对应的服务器请求数据
				//这里连flow号都跟ccd的一样，实际应用中enqueue到dcc的flow号可以根据实际需要来填写；
				mq_mcd_2_dcc->enqueue(buf, data_len, flow);
			}
			else {
				if(ccdheader->_type == ccd_rsp_send_nearly_ok) {
					printf("recv send nearly ok signal\n");
				}
			}
		}
	}while(ret == 0 && data_len > 0);
}
void CHttpApp::dispatch_dcc()
{
	unsigned data_len;
	unsigned flow;
	int ret;
	
	do
	{
		data_len = 0;
		//这里使用try_dequeue，永远不会阻塞
		ret = mq_dcc_2_mcd->try_dequeue(buf, 1<<24, data_len, flow);
		if(!ret && data_len > 0) {
#ifdef _SHMMEM_ALLOC_			
			//设置传输数据的方式
			//因为这里把dcc回复的数据直接透传给了ccd，所以虽然dcc回复的方式dcc_rsp_data_shm，但是mcd
			//并没有释放响应的共享内存，而是把释放内存的操作延迟到ccd来做
			if(dccheader->_type == dcc_rsp_data_shm) {
				ccdheader->_type = ccd_req_data_shm;
				printf("recv from dcc shmalloc data\n");
			}	
			else	
#endif
				ccdheader->_type = ccd_req_data;
			mq_mcd_2_ccd->enqueue(buf, data_len, flow);
		}
	}while(ret == 0 && data_len > 0);
}

void disp_ccd(void* priv)
{
	CHttpApp* app = (CHttpApp*)priv;
	app->dispatch_ccd();
}
void disp_dcc(void* priv)
{
	CHttpApp* app = (CHttpApp*)priv;
	app->dispatch_dcc();
}
extern "C"
{
	CacheProc* create_app()
	{
		return new CHttpApp();
	}
}
