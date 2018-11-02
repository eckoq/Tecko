#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>

#include "tfc_base_config_file.h"
#include "log_type.h"
//#include "log_info.h"
#include "stat.h"
#include "log_client.h"

using namespace std;
using namespace tools;

#define INFOLEN 128
#define	TRYGAP	1

#define	GET_EVENT(event_type)					(((event_type) >> EVT_SFT) & 0xFF)
#define GET_TRIGGER(event_type)                                 (((event_type) >> TRG_SFT) & 0xFF)

char version[]="mcp_trace 1.0.0";

//
string	_confPath="";
int	_stat=0;
int	_level=2;
string	_type = "";
vector<int> _event;
unsigned _remoteHost=0;
unsigned _remotePort=0;
unsigned _localHost=0;
unsigned _localPort=0;
short	 _mq=-1;
int	 _detail=0;
vector<string>	_mqNames;
map<unsigned, string> _errMsg;
unsigned _curType = 0;

char* _eventNames[]={"EstabConnect",
	"CloseConnect",
	"RecvPackage",
	"SendPackage",
	"DropPackage",
	"overload",
	"GrowMem",
	"ShrinkMem",
	"Enqueue",
	"Dequeue",
	"TnsEvent"};

CLogClient _logClient;

void showVersion()
{
	printf("Version:\t%s\n", version);
#ifdef BUILETIME
	printf("Build:\t%s\n", BUILETIME);
#endif
	exit(0);
}

void showHelp()
{
	printf("Usage: mcp_trace [-s statgap] [-d] [-l level] [-e event] [option] -f conf\n");
	printf("\nUse CTRL+C to quit\n\n");
	printf("\t-v --version\n");
	printf("\t\tShow version.\n\n");
	printf("\t-f --conf=CONF\n");
	printf("\t\tCONF would be configure file of mcd, ccd or dcc.\n\n");
	printf("\t-s --stat=STATGAP\n");
	printf("\t\tPrint statistics in STATGAP seconds.\n\n");
	printf("\t-d --detail\n");
	printf("\t\tPrint detail of error.when use '--stat' option, it will be invalid.\n\n");
	printf("\t-l --level=(0|1|2)\n");
	printf("\t\tPrint log which's level greater or equal to 'level'. level is 2 by default. Can't be used with '--stat' option.\n\n");
	printf("\t-e --event=EVENT(EstabConnect|CloseConnect|SendPackage|DropPackage\n"); /*RecvPackage not avail*/
	printf("\t\t\t|GrowMem|ShrinkMem|Enqueue|Dequeue|TnsEvent)\n");
	printf("\t\tPrint log about the EVENT. You can specify more than one EVENT, with separator '|'. If specify 'SendPackage', print error of send. when use '--stat' option, it will be invalid.\n\n");
	printf("option:\n");
	printf("\t--remotehost=IP\n");
	printf("\t\tPrint log only if remote_ip is IP. when use '--stat' option, it will be invalid.\n\n");
	printf("\t--remoteport=PORT\n");
	printf("\t\tPrint log only if remote_port is PORT. when use '--stat' option, it will be invalid.\n\n");
	printf("\t--localhost=IP\n");
	printf("\t\tPrint log only if local_ip is IP. when use '--stat' option, it will be invalid.\n\n");
	printf("\t--localport=PORT\n");
	printf("\t\tPrint log only if local_port is PORT. when use '--stat' option, it will be invalid.\n\n");
	printf("\t--mq=NAME\n");
	printf("\t\tPrint log only if the name of mq is 'NAME'. NAME should be the same as being in configure.\n\n");


	exit(0);
}

int loadConf()
{

	if(_confPath.find("ccd")!=string::npos || _confPath.find("dcc")!=string::npos)
	{
		char mq_name[64] = {0};
		for(unsigned i=0; i<32; i++)
		{
			if(i%2==0)
			{
				if(i==0)
					strcpy(mq_name, "req_mq_conf");
				else
					sprintf(mq_name, "req_mq%u_conf", i/2 + 1);
			}
			else
			{
				if(i==1)
					strcpy(mq_name, "rsp_mq_conf");
				else
					sprintf(mq_name, "rsp_mq%u_conf", i/2 + 1);
			}
			_mqNames.push_back(mq_name);
		}
	}
	else
	{
		tfc::base::CFileConfig page;
		page.Init(_confPath);

		const map<string, string>& mqs = page.GetPairs("root\\mq");
		for(map<string, string>::const_iterator it=mqs.begin(); it!=mqs.end(); it++)
		{
			_mqNames.push_back(it->first);
		}
	}
	return 0;
}

void getEventId(char* events)
{
	if(!events) return;

	char* tmp = strdupa(events);

	for(char* str=tmp;;str=NULL)
	{
		char* event = strtok(str, "|");
		if(!event) break;

		if(strcmp(event, "EstabConnect")==0)
			_event.push_back(LOG_NET_ESTAB);
		else if(strcmp(event, "CloseConnect")==0)
			_event.push_back(LOG_NET_CLOSE);
		else if(strcmp(event, "RecvPackage")==0)
			_event.push_back(LOG_NET_RECV);
		else if(strcmp(event, "SendPackage")==0)
			_event.push_back(LOG_NET_SEND);
		else if(strcmp(event, "DropPackage")==0)
			_event.push_back(LOG_NET_DROP);
		else if(strcmp(event, "GrowMem")==0)
			_event.push_back(LOG_MEM_GROW);
		else if(strcmp(event, "ShrinkMem")==0)
			_event.push_back(LOG_MEM_SHRINK);
		else if(strcmp(event, "Enqueue")==0)
			_event.push_back(LOG_MQ_ENQ);
		else if(strcmp(event, "Dequeue")==0)
			_event.push_back(LOG_MQ_DEQ);
		else if(strcmp(event, "TnsEvent")==0)
			_event.push_back(LOG_NS_EVT);
	}
}

void printHeadTitle()
{
	printf("\n%15s%13s%13s", "time", "event", "trigger");
}

void printHead(struct timeval& t,unsigned event)
{
	time_t l_time = (time_t)t.tv_sec;
	struct tm stTm ;
	localtime_r(&l_time, &stTm);

	printf("%02d:%02d:%02d.%06u%13s%13s",
			stTm.tm_hour, stTm.tm_min, stTm.tm_sec, (unsigned)t.tv_usec,
			_eventNames[GET_EVENT(event)-1], GET_TRIGGER(event)>0?_eventNames[GET_TRIGGER(event)-1]:"NA");
}

void printTailTitle()
{
	if(_detail)
		printf(" errMsg\n");
	else
		printf("\n");
}

void printTail(unsigned event)
{
	if(_detail && _errMsg.find(event) != _errMsg.end())
		printf(" %s\n", _errMsg[event].c_str());
	else
		printf("\n");
}

void initErrMsg()
{
	_errMsg[LOG_ESTAB_CONN           ]="Establish connection";
	_errMsg[LOG_NO_FREE_FLOW         ]="Find free flow fail";
	_errMsg[LOG_NO_FREE_CC           ]="No free conncache";
	_errMsg[LOG_SOCK_CONN_FAIL       ]="connect() system call fail";
	_errMsg[LOG_OVERLOAD_RJT_CONN    ]="Loadgrid full, reject connection";
	_errMsg[LOG_NO_COMP_FUNC         ]="No complete func for tcp";
	_errMsg[LOG_SYNC_REQ_FAIL        ]="Sync request fail";
	_errMsg[LOG_SYNC_RSP_FAIL        ]="Sync response fail";
	_errMsg[LOG_DATA_TOO_LARGE       ]="Datalen > buffer_size after packet complete check ";
	_errMsg[LOG_COMP_CHK_FAIL        ]="Packet complete check fail";
	_errMsg[LOG_EPOLL_ERR            ]="Events error when handle socket";
	_errMsg[LOG_FLOW_CONFLICT        ]="flow conflict (ip/port not match)";
	_errMsg[LOG_CONN_TYPE_CONFLICT   ]="connection type conflict";
	_errMsg[LOG_SEND_DATA            ]="Send data";
	_errMsg[LOG_SEND_ERR_RETRY       ]="Send error, retry";
	_errMsg[LOG_SEND_FORCE_ERR       ]="SendForce fail";
	_errMsg[LOG_SEND_ERR_CLOSE       ]="Send error, close connection";
	_errMsg[LOG_SEND_BUF_FULL_CLOSE  ]="Send buffer max reach, close connection";
	_errMsg[LOG_SEND_ERR_DROP        ]="UDP send error";
	_errMsg[LOG_SEND_BUF_FULL_DROP   ]="Send buffer max reach";
	_errMsg[LOG_RECV_DATA            ]="Recv data";
	_errMsg[LOG_RECV_ERR_CLOSE       ]="TCP recv error, close connection";
	_errMsg[LOG_RECV_BUF_FULL_CLOSE  ]="recv buffer max reach, close connection";
	_errMsg[LOG_RECV_ERR_DROP        ]="UDP recv error";
	_errMsg[LOG_FLOW2CC_NOT_MATCH    ]="No cc found";
	_errMsg[LOG_INVALID_CONN_TYPE    ]="Invalid connection type";
	_errMsg[LOG_DEQ_DATA             ]="Dequeue data";
	_errMsg[LOG_DATA_EXPIRE          ]="Data expire in queue.";
	_errMsg[LOG_ENQ_DATA             ]="Enqueue data";
	_errMsg[LOG_ENQ_FAIL_DROP        ]="Enqueue fail";
	_errMsg[LOG_ENQ_FAIL_CLOSE       ]="Enqueue fail, close connection";
	_errMsg[LOG_OVERLOAD_CLOSE       ]="Loadgrid full! Close connection. (ip,port,port,conn_num)";
	_errMsg[LOG_OVERLOAD_DROP        ]="Loadgrid full! Drop packet. (udp_port)";

	_errMsg[LOG_RECV_OOM             ]="Memory overload in recv";
	_errMsg[LOG_SEND_OOM             ]="Memory overload in send";

	_errMsg[LOG_MCD_ENQ              ]="Enqueue data";
	_errMsg[LOG_MCD_DEQ              ]="Dequeue data";
	_errMsg[LOG_MCD_ENQ_FAIL         ]="Enqueue fail";
	_errMsg[LOG_MCD_DEQ_DATA_EXPIRE  ]="Dequeue data expire";
	_errMsg[LOG_MCD_FIND_NO_TIMER    ]="Find timer obj fail";

	_errMsg[LOG_MCD_NS_GET           ]="TNS get event";
	_errMsg[LOG_MCD_NS_REG_DATA_CHG  ]="TNS register data change event";
	_errMsg[LOG_MCD_NS_DATA_CHG      ]="TNS data change event";
	_errMsg[LOG_MCD_NS_GET_FAIL      ]="TNS get failed";
	_errMsg[LOG_MCD_NS_DEC_FAIL      ]="decode hashmap failed";
	_errMsg[LOG_MCD_NS_CHK_FAIL      ]="Check hashmap header failed";
	_errMsg[LOG_MCD_NS_FIND_NO_SVR   ]="Refresh hashmap, but cannot find server";
	_errMsg[LOG_MCD_NS_FIND_NO_PRX   ]="Search proxy failed";
	_errMsg[LOG_MCD_NS_RETRY_REG     ]="Retry register failed";
	_errMsg[LOG_MCD_NS_NO_PRX_ID     ]="Alloc proxy id failed";
}

	template<class T>
void printLine(int event, struct timeval& timestamp, T& info)
{
	if(info.filter(_remoteHost, _remotePort, _localHost, _localPort, _mq))
	{
		if(_curType!=GET_LOGTYPE(event))
		{
			printHeadTitle();
			info.printTitle();
			printTailTitle();
			_curType=GET_LOGTYPE(event);
		}

		printHead(timestamp, event);
		info.printItem(_mqNames);
		printTail(event);
	}
}

void printLog()
{
	//read
	unsigned event;
	struct timeval timestamp;
	void* log_info;

	while(1)
	{
		int ret = _logClient.read_log(event, timestamp, log_info);
		if(ret != 0)
		{
			sleep(TRYGAP);
			continue;
		}

		//fileter
		if(_event.size() != 0)
		{
			ret = 0;
			for(unsigned i=0; i<_event.size(); i++)
			{
				ret = EVENT_MATCH(event,(unsigned)_event[i]);
				if(ret)
					break;

			}
			if(ret==0)
				continue;
		}
		//print

		switch(GET_LOGTYPE(event))
		{
			case LOG4NET:
				{
					NetLogInfo* info = (NetLogInfo*)log_info;
					printLine(event, timestamp, *info);
				}
				break;
			case LOG4MEM:
				{
					MemLogInfo* info = (MemLogInfo*)log_info;
					printLine(event, timestamp, *info);
				}
				break;
			case LOG4MCD:
				{
					MCDLogInfo* info = (MCDLogInfo*)log_info;
					printLine(event, timestamp, *info);
				}
				break;
			case LOG4NS:
				{
					NSLogInfo* info = (NSLogInfo*)log_info;
					printLine(event, timestamp, *info);
				}
			default:
				break;
		}
	}
}

void printStat(unsigned type)
{
	void* stat_info;

	switch(type)
	{
		case STAT_NET:
			CCDStatInfo::printTitle();
			break;
		case STAT_MCD:
			MCDStatInfo::printTitle();
			break;
		default:
			break;
	}

	while(1)
	{
		if(_logClient.read_stat(type, stat_info))
		{
			sleep(TRYGAP);
			continue;
		}

		switch(type)
		{
			case STAT_NET:
				((CCDStatInfo*)stat_info)->printItem();
				break;
			case STAT_MCD:
				((MCDStatInfo*)stat_info)->printItem();
				break;
			default:
				break;
		}

		sleep(_stat);
	}
}

void signal_handler(int sig)
{
	if(_logClient.is_inited())
	{
		_logClient.change_log_level(3);
		_logClient.change_stat_gap(0);
	}

	printf("exit with signal %d\n", sig);
	exit(0);
}

int main(int argc, char* argv[])
{
	enum
	{
		VER = 1,
		CONF,
		STAT,
		DETAIL,
		LEVEL,
		TYPE,
		EVENT,
		REMOTEHOST,
		REMOTEPORT,
		LOCALHOST,
		LOCALPORT,
		MQ
	};

	struct option long_options[] = {
		{ "version", 0, NULL, VER},
		{ "conf", 1, NULL, CONF},
		{ "stat", 1, NULL, STAT},
		{ "detail", 0, NULL, STAT},
		{ "level", 1, NULL, LEVEL},
		{ "type", 1, NULL, TYPE},
		{ "event", 1, NULL, EVENT},
		{ "remotehost", 1, NULL, REMOTEHOST},
		{ "remoteport", 1, NULL, REMOTEPORT},
		{ "localhost", 1, NULL, LOCALHOST},
		{ "localport", 1, NULL, LOCALPORT},
		{ "mq", 1, NULL, MQ},
		{ NULL , 0, NULL, 0}
	};

	int c;
	string mqName="";
	while((c = getopt_long (argc, argv, "vs:dl:t:e:f:", long_options, NULL)) != -1)
	{
		switch(c)
		{
			case 'v':
			case VER:
				showVersion();
				break;
			case 'f':
			case CONF:
				_confPath = optarg;
				break;
			case 's':
			case STAT:
				_stat = atoi(optarg);
				break;
			case 'd':
			case DETAIL:
				_detail = 1;
				break;
			case 'l':
			case LEVEL:
				_level = atoi(optarg);
				break;
			case 't':
			case TYPE:
				_type = optarg;
				break;
			case 'e':
			case EVENT:
				getEventId(optarg);
				break;
			case REMOTEHOST:
				_remoteHost = inet_addr(optarg);
				break;
			case REMOTEPORT:
				_remotePort = atoi(optarg);
				break;
			case LOCALHOST:
				_localHost = inet_addr(optarg);
				break;
			case LOCALPORT:
				_localPort = atoi(optarg);
				break;
			case MQ:
				mqName = optarg;
				break;
			default:
				showHelp();
				break;
		}
	}

	if(_confPath == "")
		showHelp();
	if(loadConf())
	{
		printf("load conf fail");
		exit(0);
	}

	if(mqName != "")
	{
		for(unsigned i=0; i<_mqNames.size(); i++)
		{
			if(mqName == _mqNames[i])
			{
				_mq = i;
				break;
			}
		}
	}


	initErrMsg();

	_logClient.init(_confPath);

	signal(SIGINT, signal_handler);

	if(_stat)
	{
		_logClient.change_stat_gap(_stat);
		if(_confPath.find("ccd")!=string::npos || _confPath.find("dcc")!=string::npos)
			printStat(STAT_NET);
		else
			printStat(STAT_MCD);
	}
	else
	{
		_logClient.change_log_level(_level);
		printLog();
	}
}


