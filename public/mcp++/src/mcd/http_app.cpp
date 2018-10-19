#include "tfc_cache_proc.h"
#include "tfc_net_ccd_define.h"
#include "myalloc.h"
#include <stdio.h>
#include <string.h>

using namespace tfc::cache;
using namespace tfc::net;

void disp_ccd(void* priv);

class CHttpApp : public CacheProc
{
public:
	CHttpApp(){}
	virtual ~CHttpApp(){}
	virtual void run(const std::string& conf_file);
	void get_test_file(char* filename, char* html_content, unsigned& html_content_len);
	void dispatch_ccd();

	CFifoSyncMQ* mq_ccd_2_mcd;
	CFifoSyncMQ* mq_mcd_2_ccd;
	TCCDHeader* ccdheader;
	char html_content[1<<24];
	unsigned html_content_len;
	char buf[1<<20];
};

void CHttpApp::get_test_file(char* filename, char* html_content, unsigned& html_content_len) 
{
	FILE *pf = fopen(filename, "r");
	if(pf) {
		html_content_len = fread(html_content + 1024, 1, 10000000, pf); 	
		fclose(pf);
		int head_len = sprintf(html_content, "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nContent-Length: %u\r\n\r\n", html_content_len);
		memmove(html_content + head_len, html_content + 1024, html_content_len);
		html_content_len += head_len;
	}
}
void CHttpApp::run(const std::string& conf_file)
{
	mq_ccd_2_mcd = _mqs["mq_ccd_2_mcd"];
	mq_mcd_2_ccd = _mqs["mq_mcd_2_ccd"];
	
	ccdheader = (TCCDHeader*)html_content;
	get_test_file("./tmp/test_file.html", (char*)html_content + CCD_HEADER_LEN, html_content_len);

	//添加mq_ccd_2_mcd到epoll监控
	add_mq_2_epoll(mq_ccd_2_mcd, disp_ccd, this);

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
	//static bool send_flag = false;
	do
	{
		data_len = 0;
		//这里使用try_dequeue，永远不会阻塞
		ret = mq_ccd_2_mcd->try_dequeue(buf, 1<<20, data_len, flow);
		if(!ret && data_len > 0) {
			TCCDHeader* head = (TCCDHeader*)buf;
			if(head->_type == ccd_rsp_data) {
#ifdef _SPEEDLIMIT_
				//演示设置下载速率
				ccdheader->_type = ccd_req_set_dspeed;
				ccdheader->_arg = 64; 
				mq_mcd_2_ccd->enqueue((char*)ccdheader, CCD_HEADER_LEN, flow);
#endif			
				ccdheader->_type = ccd_req_data;						
				mq_mcd_2_ccd->enqueue(html_content, CCD_HEADER_LEN + html_content_len, flow);	
				//mq_mcd_2_ccd->enqueue(html_content, CCD_HEADER_LEN + html_content_len / 2, flow);	
				//memmove((char*)html_content + CCD_HEADER_LEN, (char*)html_content + CCD_HEADER_LEN + html_content_len / 2, html_content_len - html_content_len / 2);
				
				//主动断开连接
				//ccdheader->_type = ccd_req_disconnect;
				//mq_mcd_2_ccd->enqueue((char*)ccdheader, CCD_HEADER_LEN, flow);
			
			}
			else if(head->_type == ccd_rsp_send_nearly_ok) {
				//if(!send_flag) {
				//	mq_mcd_2_ccd->enqueue(html_content, CCD_HEADER_LEN + html_content_len - html_content_len / 2, flow);	
				//	send_flag = true;
				//}
			}
		}
	}while(ret == 0 && data_len > 0);
}
void disp_ccd(void *priv)
{
	CHttpApp* app = (CHttpApp*)priv;
	app->dispatch_ccd();
}
extern "C"
{
	CacheProc* create_app()
	{
		return new CHttpApp();
	}
}
