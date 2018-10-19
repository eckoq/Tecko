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
	ALARM_TYPE_CLIENT = 1, //�ͻ��˸澯
	ALARM_TYPE_MASTER  = 2  //master�������澯
};

enum
{
	CMD_STAT_REPORT				= 0x10000, //�ͻ��˳�����Ϣ�ϱ�
	CMD_ALARM_REPORT			= 0x10002, //�ͻ��˸澯
	CMD_UPGRADE_NOTIFY			= 0x10003, //master ֪ͨweb server����
	CMD_UPGRADE_RESLUT_REPORT	= 0x10004, //��������ϱ�
	CMD_ADD_ATTR				= 0x10005,
	CMD_DEL_ATTR				= 0x10006,
	CMD_UPDATE_ATTR				= 0x10007,
	CMD_GET_BASE_INFO			= 0x10008,
};

typedef struct
{
	unsigned int length;		//Э�鳤�ȣ���������
	union {
		unsigned int cmd;		//�����֣�����Э��ͷ��
		unsigned int result;	//���ֵ���ظ�Э��ͷ��
	};
	char body[0];			//Э����
}proto_header;

//web server�ϱ�Э�����
typedef struct
{
	unsigned int ip;			//nws ip
	unsigned char pid;			//���������ʶ��1-nws��2-mcp
	unsigned char type;			//�ϱ��������ͣ�1-base info��2��attributes
	unsigned short length;		//���ݳ���
	char desc[0];				//������Ϣ���ݣ���ʽ�Զ���
	/***************
	base info��ʽ��ÿ�γ����������ϱ�
	<ports>80,8080</ports><qnf>qnf.3.1</qnf><plugin>music.so</plugin><prog_path>/usr/local/nws/bin</prog_path><conf_path>/usr/local/nws/etc</conf_path>
	attributes ��ʽ�������ϱ�
	imgcache.qq.com-404:20000,qqlive.qq.com-404:500
	****************/
}stat_report_req;

//�澯��ϢЭ�����
typedef struct
{
	unsigned int ip;
	unsigned int time;
	unsigned char level; //��1-5����1��Ϊ������
	unsigned char type;	// ALARM_TYPE_CLIENT
	char msg[256];
}alarm_report_req;

//���ԣ�id��value����
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

//�����ʵ�ֵĽӿڶ���
int get_attrs(char *data,int length,map<string,unsigned short> &attr_conf,vector<attr_value> &vt_attrs);
int get_base_info(char *data,int length,base_info &base);

__END_DECLS

#endif

#pragma pack()

#endif
