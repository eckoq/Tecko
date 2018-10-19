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

	//����TCCDHeader��TDCCHeader�ĳ�����ͬ�����������ͷ��ָ����ͬһ��������
	ccdheader = (TCCDHeader*)buf;
	dccheader = (TDCCHeader*)buf;	
	
	//���mq_ccd_2_mcd��epoll���
	add_mq_2_epoll(mq_ccd_2_mcd, disp_ccd, this);
	//���mq_dcc_2_mcd��epoll���
	add_mq_2_epoll(mq_dcc_2_mcd, disp_dcc, this);

	fprintf(stderr, "mcd so started\n");
	//��ѭ��
	while(!stop) {
		
		//���mq�¼���������Ӧ�Ļص�����
		run_epoll_4_mq();	
		
		//����������
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
		//����ʹ��try_dequeue����Զ��������
		ret = mq_ccd_2_mcd->try_dequeue(buf, 1<<24, data_len, flow);
		if(!ret && data_len > 0) {
			if(ccdheader->_type == ccd_rsp_data || ccdheader->_type == ccd_rsp_data_shm) {

				//Ϊ���Է��㣬Ӳ�����˺�̨������ip��port
				dccheader->_ip = (unsigned)inet_addr("10.1.252.12");
				dccheader->_port = 20000;
				//���ô������ݵķ�ʽ
#ifdef _SHMMEM_ALLOC_
				if(ccdheader->_type == ccd_rsp_data_shm) {
					dccheader->_type = dcc_req_send_shm;
					printf("recv from ccd shmalloc data\n");
				}	
				else				
#endif
					dccheader->_type = dcc_req_send; 

				//����ֱ�Ӱ�ccd������������ԭ�ⲻ��ת����dccȥ������dccȥ��Ӧ�ķ�������������
				//������flow�Ŷ���ccd��һ����ʵ��Ӧ����enqueue��dcc��flow�ſ��Ը���ʵ����Ҫ����д��
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
		//����ʹ��try_dequeue����Զ��������
		ret = mq_dcc_2_mcd->try_dequeue(buf, 1<<24, data_len, flow);
		if(!ret && data_len > 0) {
#ifdef _SHMMEM_ALLOC_			
			//���ô������ݵķ�ʽ
			//��Ϊ�����dcc�ظ�������ֱ��͸������ccd��������Ȼdcc�ظ��ķ�ʽdcc_rsp_data_shm������mcd
			//��û���ͷ���Ӧ�Ĺ����ڴ棬���ǰ��ͷ��ڴ�Ĳ����ӳٵ�ccd����
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
