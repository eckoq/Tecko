
#ifndef __TFC_BASE_LOG_H__
#define __TFC_BASE_LOG_H__
 
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <fstream>
#include <stdexcept>
using namespace std;

namespace tfc { namespace base {

struct log_open_fail: public runtime_error{ log_open_fail(const string& s);};

class CRollLog; class CDayLog;
/**
 * 参见tfc::base::CRollLog
 * @see #tfc::base::CRollLog
 */
typedef CRollLog& (*__Roll_Func)(CRollLog&);
/**
 * 参见tfc::base::CDayLog
 * @see #tfc::base::CDayLog
 */
typedef CDayLog& (*__Day_Func)(CDayLog&);

/**
 * 滚动日志类,用法与cout/cerr相同
 * <p>注意使用endl结束一行,目的是判断是否需要加时间戳等</p>
 * <p>Copyright: Copyright (c) 2004</p>
 * <p>Create: 2004-11-15</p>
 * <p>LastModify: 2004-12-14</p>
 * @author  casper@tencent.com
 * @version  0.4
 * @since version 0.3
 */
class CRollLog
{
public:
	CRollLog();
	/**
	 * no implementation
	 */
	CRollLog(const CRollLog&);
	~CRollLog();

public:
	/**
	 * log级别
	 */
	enum Log_Level{
		NO_LOG = 1,  
		ERROR_LOG = 2,  
		NORMAL_LOG = 3, 
		DEBUG_LOG = 4  
	};
	/**
	 * 填充字段定义:时间/模块名/pid/debug信息提示
	 */
	enum title_flag {
		F_Time = 0,
		F_Module = 1,
		F_PID = 2,
		F_DEBUGTIP = 3
	};

public:
	/**
	 * 滚动日志初始化
	 * @throw log_open_fail when file cann't been opened
	 * @param sPreFix 滚动日志的前缀,会自动补上.log
	 * @param module 模块名,调用者模块名,可以记入log文件中
	 * @param maxsize 一个日志文件的最大值
	 * @param maxnum 滚动日志的数量,旧的文件自动更新为sPreFix(i).log
	 */
	void Init(const string& sPreFix,const string& module,size_t maxsize,size_t maxnum=10)
		throw(log_open_fail);

	/**
	 * 设置时间格式
	 * @param format 时间格式,参考linux下date命令
	 */
	void time_format(const string& format="[%Y-%m-%d %H:%M:%S]") {_time_format = format;}

	/**
	 * 设置日志级别
	 * @param Log_Level 日志级别
	 */
	void set_level(Log_Level l){_setlevel = l;}

	/**
	 * 设置行首自动填充的字段
	 * @param title_flag title字段,set之后该字段会自动添加
	 * @see #clear_titleflag
	 */
	void set_titleflag(title_flag f); 
	/**
	 * 清除行首自动填充的字段
	 * @param title_flag title字段,clear之后该字段不会自动添加
	 * @see #set_titleflag
	 */
	void clear_titleflag(title_flag f) {_flags[f] = false;}

public:
	CRollLog& operator<<(char n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(unsigned char n) { return (*this) << (char)n;}

	CRollLog& operator<<(short n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(unsigned short n) {if(check_level()) _os<<n;return *this;}

	CRollLog& operator<<(int n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(unsigned int n) {if(check_level()) _os<<n;return *this;}

	CRollLog& operator<<(long n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(unsigned long n) {if(check_level()) _os<<n;return *this;}

	CRollLog& operator<<(long long n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(unsigned long long n) {if(check_level()) _os<<n;return *this;}

	CRollLog& operator<<(double n) {if(check_level()) _os<<n;return *this;}
	CRollLog& operator<<(float n){ return (*this)<<((double)n); }

	CRollLog& operator<<(const string& s) {if(check_level()) _os<<s;return *this;}
	CRollLog& operator<<(const char *cc) { string s(cc); if(check_level()) _os<<s;return *this;}
	CRollLog& operator<<(__Roll_Func func){ return (*func)(*this);}

protected:
	bool check_level(); 
	string cur_time();

	friend CRollLog& endl(CRollLog& log);

	friend CRollLog& debug(CRollLog& log);
	friend CRollLog& error(CRollLog& log);
	friend CRollLog& normal(CRollLog& log);
	friend CRollLog& nolog(CRollLog& log);

protected:
	static const unsigned FLUSH_COUNT = 32;

	ofstream _os;
	Log_Level _setlevel;
	Log_Level _level;
	unsigned _buf_count;

	unsigned _max_log_size;
	unsigned _max_log_num;

	string _module;
	string _filename;
	string _time_format;
	unsigned _pid;
	string _debugtip;

	bool _flags[4];
	bool _lock;
};

/**
 * 按天日志类,用法与cout/cerr相同
 * <p>注意使用endl结束一行,目的是判断是否需要加时间戳等</p>
 * <p>Copyright: Copyright (c) 2004</p>
 * <p>Create: 2004-11-15</p>
 * <p>LastModify: 2004-12-14</p>
 * @author  casper@tencent.com
 * @version  0.4
 * @since version 0.3
 */
class CDayLog
{
public:
	CDayLog();
	/**
	 * no implementation
	 */
	CDayLog(const CDayLog&);
	~CDayLog();

public:
	/**
	 * 填充字段定义:时间/模块名/pid
	 */
	enum title_flag {
		F_Time = 0,
		F_Module = 1,
		F_PID = 2
	};

public:

	/**
	 * 按天日志初始化
	 * @throw log_open_fail when file cann't been opened
	 * @param sPreFix 按天日志的前缀,会自动补上_YYYYMMDD.log
	 * @param module 模块名,调用者模块名,可以记入log文件中
	 */
	void Init(const string& sPreFix,const string& module) throw(log_open_fail);

	/**
	 * 设置时间格式
	 * @param format 时间格式,参考linux下date命令
	 */
	void time_format(const string& format="[%H:%M:%S]") {_time_format = format;}

	/**
	 * 设置行首自动填充的字段
	 * @param title_flag title字段,set之后该字段会自动添加
	 * @see #clear_titleflag
	 */
	void set_titleflag(title_flag f);

	/**
	 * 清除行首自动填充的字段
	 * @param title_flag title字段,clear之后该字段不会自动添加
	 * @see #set_titleflag
	 */
	void clear_titleflag(title_flag f) {_flags[f] = false;}

public:
	CDayLog& operator<<(char n);
	CDayLog& operator<<(unsigned char n);

	CDayLog& operator<<(short n);
	CDayLog& operator<<(unsigned short n);

	CDayLog& operator<<(int n);
	CDayLog& operator<<(unsigned int n);

	CDayLog& operator<<(long n);
	CDayLog& operator<<(unsigned long n);

	CDayLog& operator<<(long long n);
	CDayLog& operator<<(unsigned long long n);

	CDayLog& operator<<(double n);
	CDayLog& operator<<(float n);

	CDayLog& operator<<(const string& s);
	CDayLog& operator<<(const char *cc);
	CDayLog& operator<<(__Day_Func func){ return (*func)(*this);}

protected:
	string s_time(time_t t);
	string s_day(time_t t);
	time_t t_day(time_t t);
	bool is_same_day(time_t t1,time_t t2);
	bool Lock();
	void UnLock();//{if(_lock) {if(_os.is_open()) _os<<unlock; _lock = false;} }

	friend CDayLog& endl(CDayLog& log);
protected:
	static const unsigned FLUSH_COUNT = 32;

//	ofstream _os;
	int _fd;

	string _module;
	string _filename;
	string _time_format;
	unsigned _pid;

	bool _flags[3];
	bool _lock;

	time_t _last;
};

/**
 * 滚动日志设置log为debug信息,用法 rolllog << debug
 * @see #tfc::base::CRollLog
 */
inline CRollLog& debug(CRollLog& log)
{
	log._debugtip = "DEBUG:";
	log._level = CRollLog::DEBUG_LOG;
	return log;
}

/**
 * 滚动日志设置log为normal信息,用法 rolllog << normal
 * @see #tfc::base::CRollLog
 */
inline CRollLog& normal(CRollLog& log)
{
	log._debugtip = "";
	log._level = CRollLog::NORMAL_LOG;
	return log;
}

/**
 * 滚动日志设置log为error信息,用法 rolllog << error
 * @see #tfc::base::CRollLog
 */
inline CRollLog& error(CRollLog& log)
{
	log._debugtip = "ERROR:";
	log._level = CRollLog::ERROR_LOG;
	return log;
}

/**
 * 滚动日志设置log为nolog信息,用法 rolllog << nolog
 * @see #tfc::base::CRollLog
 */
inline CRollLog& nolog(CRollLog& log)
{
	log._debugtip = "";
	log._level = CRollLog::NO_LOG;
	return log;
}

/**
 * 滚动日志设置一行结束,用法 rolllog << endl
 * @see #tfc::base::CRollLog
 */
inline CRollLog& endl(CRollLog& log)
{
	if(log.check_level()) log._os << std::endl;
	log._lock = false;
	return log;
}

/**
 * 按天日志设置一行结束,用法 daylog << endl
 * @see #tfc::base::CRollLog
 */
inline CDayLog& endl(CDayLog& log)
{
	if(!log.Lock()) return log;
	char tmpbuf[2] = {"\n"};
	write(log._fd,tmpbuf,1);
	log.UnLock();
	return log;
}

}}
#endif //


