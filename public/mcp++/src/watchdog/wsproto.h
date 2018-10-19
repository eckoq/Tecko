#ifndef __WS_PROTO_H__
#define __WS_PROTO_H__

#include <sys/cdefs.h>

#ifdef __cplusplus

#include <string>
#include <vector>

using namespace std;

#endif

#pragma pack(1)

enum
{
	STATUS_ACTIVE = 0,
	STATUS_TIMEOUT,
	STATUS_EXCEPT
};

enum
{
	ALARM_TYPE_CLIENT = 1, //客户端告警
	ALARM_TYPE_MASTER  = 2  //master服务器告警
};

enum
{
	CMD_STAT_REPORT				= 0x10000, //客户端程序信息上报
	CMD_ALARM_REPORT			= 0x10002, //客户端告警
	CMD_UPGRADE_NOTIFY			= 0x10003, //master 通知web server升级
	CMD_UPGRADE_RESLUT_REPORT	= 0x10004, //升级结果上报
	CMD_ADD_ATTR				= 0x10005,
	CMD_DEL_ATTR				= 0x10006,
	CMD_UPDATE_ATTR				= 0x10007,
	CMD_GET_BASE_INFO			= 0x10008,
};

typedef struct
{
	unsigned int length;		//协议长度（包括自身）
	union {
		unsigned int cmd;		//命令字（请求协议头）
		unsigned int result;	//结果值（回复协议头）
	};
	char body[0];			//协议体
}proto_header;

//web server上报协议包体
typedef struct
{
	unsigned int ip;			//nws ip
	unsigned char pid;			//解析插件标识，1-nws，2-mcp
	unsigned char type;			//上报内容类型，1-base info，2，attributes
	unsigned short length;		//内容长度
	char desc[0];				//描述信息内容，格式自定义
	/***************
	base info格式：每次程序启动后上报
	<ports>80,8080</ports><qnf>qnf.3.1</qnf><plugin>music.so</plugin><prog_path>/usr/local/nws/bin</prog_path><conf_path>/usr/local/nws/etc</conf_path>
	attributes 格式：定期上报
	imgcache.qq.com-404:20000,qqlive.qq.com-404:500
	****************/
}stat_report_req;

//告警信息协议包体
typedef struct
{
	unsigned int ip;
	unsigned int time;
	unsigned char level; //分1-5级，1级为最严重
	unsigned char type;	// ALARM_TYPE_CLIENT
	char msg[256];
}alarm_report_req;

//特性（id，value）对
typedef struct
{
	unsigned short attr_id;
	unsigned long attr_val;
}attr_value;

#ifdef __cplusplus

typedef struct
{
	string ports;//div by ","
	unsigned long qnf_ver;
	string plugins; //div by ";"
	string prog_path;
	string conf_path;
}base_info;

__BEGIN_DECLS

//插件需实现的接口定义
int get_attrs(char *data,int length,map<string,unsigned short> &attr_conf,vector<attr_value> &vt_attrs);
int get_base_info(char *data,int length,base_info &base);

__END_DECLS

#endif

#pragma pack()

#endif
