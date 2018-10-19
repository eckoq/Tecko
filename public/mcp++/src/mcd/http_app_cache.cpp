/*
 * 简单例子1，只有CCD+MCD的应用
 */
#include "tfc_cache_proc.h"
#include "tfc_net_ccd_define.h"
#include <stdio.h>
#include <string.h>

#define BUF_SIZE	(1<<22)

using namespace tfc::cache;
using namespace tfc::net;

void disp_ccd(void* priv);
class CHttpCacheApp : public CacheProc
{
public:
	CHttpCacheApp(){}
	virtual ~CHttpCacheApp(){}
	virtual void run(const std::string& conf_file);
	int get_filename(char* buf, unsigned buf_len, char* filename);
	int get_key(const char* filename, char* key);
	int get_file(const char* filename, char* buf, unsigned& buf_len);
	
	void dispatch_ccd();

	CFifoSyncMQ* mq_ccd_2_mcd;
	CFifoSyncMQ* mq_mcd_2_ccd;
	TCCDHeader* ccdheader;
	char send_buf[BUF_SIZE];
	char recv_buf[BUF_SIZE];
	
	tfc::diskcache::CacheAccess* cache;
};

int CHttpCacheApp::get_filename(char* buf, unsigned buf_len, char* filename) {

	char* p = strstr(buf, "GET /");
	if(!p)
		return -1;
	char* q = strstr(p, "HTTP");
	if(!q)
		return -1;
	p += 5;
	*(--q) = '\0';
	sprintf(filename, "./tmp/%s", p);		
	//printf("filename=%s\n", filename);					

	return 0;
}
int CHttpCacheApp::get_key(const char* filename, char* key) {

	int sum = 0;
	char* p = (char*)filename;
	while(*p) {
		sum += (int)*p;
		++p;	
	}
	*((int*)key) = sum;
	//printf("key=%d\n", sum);	
	return 0;
}
int CHttpCacheApp::get_file(const char* filename, char* buf, unsigned& buf_len) 
{
	buf_len = 0;

	FILE *pf = fopen(filename, "r");
	if(pf) {
		buf_len = fread(buf + 1024, 1, 10000000, pf); 	
		fclose(pf);
		int head_len = sprintf(buf, "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nContent-Length: %u\r\n\r\n", buf_len);
		memmove(buf + head_len, buf + 1024, buf_len);
		buf_len += head_len;
		
		return 0;
	}
	return -1;
}
void CHttpCacheApp::run(const std::string& conf_file)
{
	mq_ccd_2_mcd = _mqs["mq_ccd_2_mcd"];
	mq_mcd_2_ccd = _mqs["mq_mcd_2_ccd"];

	cache = _disk_caches["test_disk_cache"];

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
void CHttpCacheApp::dispatch_ccd()
{
	unsigned data_len;
	unsigned flow;
	int ret;
	unsigned send_buf_len;
	char filename[128] = {0};
	char key[16] = {0};
	bool dirty_flag;
	long time_stamp;
	// char* buf = NULL;
		
	do {
		data_len = 0;
		//这里使用try_dequeue，永远不会阻塞
		ret = mq_ccd_2_mcd->try_dequeue(recv_buf, 1<<20, data_len, flow);
		if(!ret && data_len > 0) {
			
			get_filename(recv_buf + CCD_HEADER_LEN, data_len - CCD_HEADER_LEN, filename);
			get_key(filename, key);	
			
			if(cache->get(key, send_buf + CCD_HEADER_LEN, BUF_SIZE, send_buf_len, dirty_flag, time_stamp) != 0) {			
			//if(cache->get_mmap(key, &buf, send_buf_len, dirty_flag, time_stamp) != 0) {
			//	printf("cache miss, %s\n", filename);
				get_file(filename, send_buf + CCD_HEADER_LEN, send_buf_len);
				cache->set(key, send_buf + CCD_HEADER_LEN, send_buf_len);
			}
			else {
			//	printf("cache hit, %s\n", filename);
				//memcpy(send_buf + CCD_HEADER_LEN, buf, send_buf_len);
				//cache->get_unmap(buf, send_buf_len);
			}
			
			//printf("send_len=%u\n", send_buf_len);	
			ccdheader = (TCCDHeader*)send_buf;	
			ccdheader->_type = ccd_req_data;						
			mq_mcd_2_ccd->enqueue(send_buf, CCD_HEADER_LEN + send_buf_len, flow);	
			
			//主动断开连接
			//ccdheader->_type = ccd_req_disconnect;
			//mq_mcd_2_ccd->enqueue((char*)ccdheader, CCD_HEADER_LEN, flow);
		}
	}while(ret == 0 && data_len > 0);
}
void disp_ccd(void *priv)
{
	CHttpCacheApp* app = (CHttpCacheApp*)priv;
	app->dispatch_ccd();
}
extern "C"
{
	CacheProc* create_app()
	{
		return new CHttpCacheApp();
	}
}
