#ifndef __S_PROTO_H__
#define __S_PROTO_H__

#include <stdint.h>

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
	STATUS_EXCEPT,
	STATUS_UNKNOWN
};

enum
{
	ALARM_TYPE_CLIENT = 1, //�ͻ��˸澯
	ALARM_TYPE_MASTER  = 2  //master�������澯
};

enum
{
//	CMD_STAT_REPORT				= 0x10000, //�ͻ��˳�����Ϣ�ϱ�
//	CMD_ALARM_REPORT			= 0x10002, //�ͻ��˸澯
	CMD_UPGRADE_NOTIFY			= 0x10003, //master ֪ͨweb server����
//	CMD_UPGRADE_RESLUT_REPORT	= 0x10004, //��������ϱ�
	CMD_ADD_ATTR				= 0x10005,
	CMD_DEL_ATTR				= 0x10006,
	CMD_UPDATE_ATTR				= 0x10007,
	CMD_GET_BASE_INFO			= 0x10008,
	CMD_UPGRAGE_NOTIFY_C2M		= 0x10009, //cgi 2 master
	CMD_GET_FILE_CONTENT		= 0x1000a, // Get config or log file content.
//	CMD_REPORT_FILE_CONTENT		= 0x1000b,  // Report File content.
	
	CMD_QUERY_SERVER_STATUS		= 0x10010,
	CMD_QUERY_SERVER_STATUS2	= 0x10011,

	// For IP + PORT.
	CMD_STAT_REPORT_V2			= 0x10020, //�ͻ��˳�����Ϣ�ϱ�IP + PORT.
	CMD_ALARM_REPORT_V2			= 0x10021, //�ͻ��˸澯IP + PORT
	CMD_UPGRADE_RESLUT_REPORT_V2	= 0x10022, //��������ϱ�IP + PORT
	CMD_REPORT_FILE_CONTENT_V2	= 0x10023,  // Report File content. IP + PORT

	CMD_REPORT_PROCESS_INFO		= 0x10030	// Report process information.
};

enum {
	UPDATE_RSTAT_RECV_OK =			1000,		// Recive update command from master OK.
	UPDATE_RSTAT_OK =				1001,		// OK.
	UPDATE_RSTAT_OTHER_ERR =		1002,		// Other error.
	UPDATE_RSTAT_CMD_ERR =			1003,		// Get or handle command file error.
	UPDATE_RSTAT_FILE_ERR =			1004,		// Get update files error.
	UPDATE_RSTAT_UPDATE_ERR =		1005,		// Replace update files error.
	UPDATE_RSTAT_CTL_ERR =			1006		// Restart server error.
};

typedef struct
{
	uint32_t length;		//Э�鳤�ȣ���������
	union {
		uint32_t cmd;		//�����֣�����Э��ͷ��
		uint32_t result;	//���ֵ���ظ�Э��ͷ��
	};
	char body[0];			//Э����
}proto_header;

/*
//web server�ϱ�Э�����
typedef struct
{
	uint32_t ip;			//nws ip
	uint8_t pid;			//���������ʶ��1-nws��2-mcp
	uint8_t type;			//�ϱ��������ͣ�1-base info��2��attributes
	uint16_t length;		//���ݳ���
	char desc[0];				//������Ϣ���ݣ���ʽ�Զ���
}stat_report_req;
*/

//web server�ϱ�Э����� IP + PORT
typedef struct
{
	uint32_t ip;			//nws ip
	uint16_t port;			// PORT.
	uint8_t pid;			//���������ʶ��1-nws��2-mcp
	uint8_t type;			//�ϱ��������ͣ�1-base info��2��attributes
	uint16_t length;		//���ݳ���
	char desc[0];				//������Ϣ���ݣ���ʽ�Զ���
	/***************
	base info��ʽ��ÿ�γ����������ϱ�
	<ports>80,8080</ports><qnf>qnf.3.1</qnf><plugin>music.so</plugin><prog_path>/usr/local/nws/bin</prog_path><conf_path>/usr/local/nws/etc</conf_path>
	attributes ��ʽ�������ϱ�
	imgcache.qq.com-404:20000,qqlive.qq.com-404:500
	****************/
}stat_report_req_v2;

/*
//�澯��ϢЭ�����
typedef struct
{
	uint32_t ip;
	uint32_t time;
	uint8_t level; //��1-5����1��Ϊ������
	uint8_t type;	// ALARM_TYPE_CLIENT
	char msg[256];
}alarm_report_req;
*/

//�澯��ϢЭ�����
typedef struct
{
	uint32_t ip;
	uint16_t port;			// PORT.
	uint32_t time;
	uint8_t level; //��1-5����1��Ϊ������
	uint8_t type;	// ALARM_TYPE_CLIENT
	char msg[256];
}alarm_report_req_v2;

typedef struct
{
	char frm_ver[64];
	char ports[64];//div by ","
	char plugins[256]; //div by ";"
	char prog_path[256];
	char conf_path[256];
}base_info_req;

typedef struct
{
	base_info_req bi;
	uint32_t atime;
	uint32_t status;
}base_info_ex_req;

#ifdef __cplusplus

typedef struct
{
	string key;
	uint32_t value;
}attr_p;

#endif

typedef struct
{
	uint32_t tid;
	char url[256];
	char token[64];
	int factor;
}task_base;

typedef struct
{
	task_base base;
	uint32_t target_cnt;
	uint32_t target[0];
}task_info;

/*
typedef struct
{
	uint32_t ip;
	uint32_t tid; //task id
	uint32_t status;
	uint32_t time; 
}task_status;
*/

typedef struct
{
	uint32_t ip;
	uint16_t port;
	uint32_t tid; //task id
	uint32_t status;
	uint32_t time; 
}task_status_v2;

/*
 * Get file type code.
 */
enum {
	GET_FILE_CONF =				100,	// Config file.
	GET_FILE_LOG =				101,	// Log file.
};
typedef struct {
	uint32_t 	type;					// File type. GET_FILE_CONF or GET_FILE_LOG.
	char		path[256];				// File path.
} get_file_content;

/*
 * Report file content status code.
 */
enum {
	REPORT_FILE_OK =			2000,	// OK.
	REPORT_FILE_LEAREG_ERR =	2001,	// File too large.
	REPORT_FILE_NFOUND_ERR =	2002,	// File not found.
	REPORT_FILE_EMPTY_ERR =		2003,	// File empty.
	REPORT_FILE_READ_ERR =		2004,	// File open or read error.
	REPORT_FILE_OTHER_ERR =		2005	// Other error.
};

/*
typedef struct {
	uint32_t	ip;						// Client IP.
	uint32_t	status;					// Return status.
	uint32_t	mtime;					// Last modified time.
	char	path[256];				// File path.
	uint32_t	file_len;				// Report file content length.
	char	file_content[0];		// Report file content.
} report_file_content;
*/

typedef struct {
	uint32_t	ip;						// Client IP.
	uint16_t	port;					// Client Server PORT.
	uint32_t	status;					// Return status.
	uint32_t	mtime;					// Last modified time.
	char	path[256];				// File path.
	uint32_t	file_len;				// Report file content length.
	char	file_content[0];		// Report file content.
} report_file_content_v2;

#define WTG_PROC_INFO_SIZE 1024			// Process information content buffer size.
typedef struct {
	uint32_t	ip;						// Client IP.
	uint16_t	port;					// Client Server PORT.

	char		content[WTG_PROC_INFO_SIZE];	// Process information.
												// Format:
												// KEY0##VALUE0\n
												// KEY1##VALUE1\n
												// \n
} report_process_info;

#ifdef __cplusplus

__BEGIN_DECLS
//�����ʵ�ֵĽӿڶ���
int get_attrs(char *desc,int length, vector<attr_p> &vt_attrs);
int get_base_info(char *desc,int length,base_info_req &bi);
__END_DECLS

#endif

#pragma pack()

#endif
