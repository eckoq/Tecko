#ifndef  __TFC_BASE_FTLIB_H
#define  __TFC_BASE_FTLIB_H
/**
 * FTLib   后端Server容错检测工具
 * Fault Tolerant Lib
 * 该工具主要功能是检测后端server断连时，自动踢除断开的server，将新请求导向其他server
 * 在分配server时兼顾各个server的处理延时，使得server之间的负载均衡
 *
 * 使用方法:
 * 1、用静态函数直接获得FTLib实例对象
 * FTLib* ftlib = FTLib::GetInstance();
 *
 * 2、调用ftlib->Init(...)接口初始化，将模块以及对应的server地址列表填充进去
 * module_name -> server list
 * "tdb_hand" -> [10.10.10.1:20000] [10.10.10.2:20000]
 * "tssd_access" -> [10.10.10.3:20000] [10.10.10.4:20000]
 *
 * 3、需要访问某个模块时，通过GetRoute接口获得一个server地址
 * FTServerAddr svrAddr;
 * string module_name = "tdb_hand";
 * ftlib->GetRoute(module_name, svrAddr);
 * 【如果业务要求某个key的处理必须在后端同一个server上串行执行，可以使用：
 * int GetRoute(unsigned key,  string module,  FTServerAddr &server);
 * FTLib内部实现能够保证相同的key，分发到相同的server】
 *
 * 4、将请求发送到该server
 * request = build_request();
 * enqueue_dcc(request, svrAddr.ip, svrAddr.port);
 *
 * 5、接收到响应后，计算该消息的处理时延，通过Report接口报给FTLib，以便FTLib均衡各个server的负载
 * int delay = recv_timestamp - send_timestamp;
 * ftlib->Report(svrAddr.ip, svrAddr.port, delay, false);
 * 如果无法连接server或者没有收到响应，将Report最后一个参数设为true，以便FTLib判断是否把该server踢掉
 *
 * 6、在程序退出的地方，调用FTLib::Release()，释放内存。
 *
 *
 * 注：
 * 该工具已经集成在MCP++发布包的lib/libtfcbase.a中，链接时需增加参数-ltfcbase
 *
 * Log:
 * version-1.0.0 2012-12-04 senyang/tommyhuang
 * version-1.0.1 2012-12-24 tommyhuang 添加最大剔除数的控制
 *
 */
#include <string>
#include <vector>
#include <map>

using namespace std;

namespace tfc { namespace base {

const float    FTLIB_DEFAULT_TTHRESHOLD = 0.1f;
const unsigned FTLIB_DEFAULT_TRETRY = 10 * 60 * 1000;   //一个已超时的server，重试的间隔时间
const unsigned FTLIB_DEFAULT_CSUCCRETRY = 5;            //连续这么多次，server回到到NORMAL状态
const unsigned FTLIB_DEFAULT_CALCSTAMP = 1 * 60 * 1000; //计算平均时延的间隔
const unsigned FTLIB_DEFAULT_KMAXRATIO = 50;			//最大剔除率
const unsigned FTLIB_MAX_TIMEOUT = 100000;   			//ms

typedef struct _FTServerAddr
{
    unsigned ip;
    unsigned short port;
} FTServerAddr;

typedef struct _FTModuleRouteTable
{
    string module_name;
    vector<FTServerAddr> server_list;
} FTModuleRouteTable;

enum FT_OPTYPE{
    ADD = 0,
    DEL = 1
};

//一个Server可能的状态
enum  FT_SERVER_STATE{
    NORMAL= 0,						//正常的可用server
    TIMEOUT = 1,					//已断开
    TIMEOUT_RECOVERING = 2,			//处于恢复中
    TIMEOUT_RECOVERING_WAITING = 3,	//已发送了的探测请求，正等待响应
};

typedef struct _FTServerStat
{
    unsigned ip;
    unsigned short port;
    unsigned conn_count;					//计算周期内的server连接次数
    unsigned timeout_count;					//计算周期内的超时次数
    unsigned total_delay; 					//计算周期内的总延迟
    unsigned avg_delay;  					//平均延迟, avg_delay = total_delay/conn_count
    FT_SERVER_STATE state;					//server当前状态
    unsigned long long expect_retry_time;	//处于TIMEOUT 状态的server期待下次开始探测的时间
    unsigned succ_retry_count;				//处于TIMEOUT_RECOVERING状态已成功重试次数
    int rnum;								//server服务范围，用于按概率确定是否选中此server
} FTServerStat;

typedef struct _FTModuleStat
{
    int total_server;				//模块总共server数
    int valid_server;				//模块可用server数
    vector<FTServerStat*> servers;	//模块中所有的server列表
} FTModuleStat;

class FTLib 
{
public:
    float tThreshold;             //超时阈值，当 超时次数/连接次数 > 阈值 时，server状态置为超时
    unsigned tRetry;              //超时server尝试重连的时间间隔
    unsigned long long expCalcTime;    //下次更新server延迟的时间
    unsigned calcStamp;           //更新server延迟的周期 curr_time+calcStamp即为下次更新的时间
    unsigned cSuccRetry;          //处于超时恢复状态server变为正常状态所需成功连接次数
    unsigned kMaxRatio;           //最大可剔除的server比例(1-100)

public:
    static FTLib* GetInstance();
    static void Release();
	
	//初始化server列表
    int Init(vector<FTModuleRouteTable> module_route_table_list);
	
	//添加或者删除server
    int Update(enum FT_OPTYPE op, string modulename, FTServerAddr server);
	
	//获取一个可用server
    int GetRoute(string module,  FTServerAddr &server);
	
	//按key获取server
    int GetRoute(unsigned key,  string module,  FTServerAddr &server);
	
	//报告server延迟
    int Report(unsigned ip, unsigned short port, int delay,  bool is_timeout);
 
    //打印所有server状态，调试使用
    void PrintAllStat();

private:
    int InitServerStat(FTServerStat* server, int rnum);
    unsigned long long GetCurrTime(); 
    FTServerStat* SearchServer(unsigned ip, unsigned short port, string &module);
    FTServerStat* GetMinDelayServer(string module);
    int RefreshStat();

private:
    FTLib(){};
    map<string, FTModuleStat> modules;
    static FTLib* ftlib;
};

}    }
#endif 
