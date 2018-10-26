
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
 * �μ�tfc::base::CRollLog
 * @see #tfc::base::CRollLog
 */
typedef CRollLog& (*__Roll_Func)(CRollLog&);
/**
 * �μ�tfc::base::CDayLog
 * @see #tfc::base::CDayLog
 */
typedef CDayLog& (*__Day_Func)(CDayLog&);

/**
 * ������־��,�÷���cout/cerr��ͬ
 * <p>ע��ʹ��endl����һ��,Ŀ�����ж��Ƿ���Ҫ��ʱ�����</p>
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
	 * log����
	 */
	enum Log_Level{
		NO_LOG = 1,  
		ERROR_LOG = 2,  
		NORMAL_LOG = 3, 
		DEBUG_LOG = 4  
	};
	/**
	 * ����ֶζ���:ʱ��/ģ����/pid/debug��Ϣ��ʾ
	 */
	enum title_flag {
		F_Time = 0,
		F_Module = 1,
		F_PID = 2,
		F_DEBUGTIP = 3
	};

public:
	/**
	 * ������־��ʼ��
	 * @throw log_open_fail when file cann't been opened
	 * @param sPreFix ������־��ǰ׺,���Զ�����.log
	 * @param module ģ����,������ģ����,���Լ���log�ļ���
	 * @param maxsize һ����־�ļ������ֵ
	 * @param maxnum ������־������,�ɵ��ļ��Զ�����ΪsPreFix(i).log
	 */
	void Init(const string& sPreFix,const string& module,size_t maxsize,size_t maxnum=10)
		throw(log_open_fail);

	/**
	 * ����ʱ���ʽ
	 * @param format ʱ���ʽ,�ο�linux��date����
	 */
	void time_format(const string& format="[%Y-%m-%d %H:%M:%S]") {_time_format = format;}

	/**
	 * ������־����
	 * @param Log_Level ��־����
	 */
	void set_level(Log_Level l){_setlevel = l;}

	/**
	 * ���������Զ������ֶ�
	 * @param title_flag title�ֶ�,set֮����ֶλ��Զ����
	 * @see #clear_titleflag
	 */
	void set_titleflag(title_flag f); 
	/**
	 * ��������Զ������ֶ�
	 * @param title_flag title�ֶ�,clear֮����ֶβ����Զ����
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
 * ������־��,�÷���cout/cerr��ͬ
 * <p>ע��ʹ��endl����һ��,Ŀ�����ж��Ƿ���Ҫ��ʱ�����</p>
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
	 * ����ֶζ���:ʱ��/ģ����/pid
	 */
	enum title_flag {
		F_Time = 0,
		F_Module = 1,
		F_PID = 2
	};

public:

	/**
	 * ������־��ʼ��
	 * @throw log_open_fail when file cann't been opened
	 * @param sPreFix ������־��ǰ׺,���Զ�����_YYYYMMDD.log
	 * @param module ģ����,������ģ����,���Լ���log�ļ���
	 */
	void Init(const string& sPreFix,const string& module) throw(log_open_fail);

	/**
	 * ����ʱ���ʽ
	 * @param format ʱ���ʽ,�ο�linux��date����
	 */
	void time_format(const string& format="[%H:%M:%S]") {_time_format = format;}

	/**
	 * ���������Զ������ֶ�
	 * @param title_flag title�ֶ�,set֮����ֶλ��Զ����
	 * @see #clear_titleflag
	 */
	void set_titleflag(title_flag f);

	/**
	 * ��������Զ������ֶ�
	 * @param title_flag title�ֶ�,clear֮����ֶβ����Զ����
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
 * ������־����logΪdebug��Ϣ,�÷� rolllog << debug
 * @see #tfc::base::CRollLog
 */
inline CRollLog& debug(CRollLog& log)
{
	log._debugtip = "DEBUG:";
	log._level = CRollLog::DEBUG_LOG;
	return log;
}

/**
 * ������־����logΪnormal��Ϣ,�÷� rolllog << normal
 * @see #tfc::base::CRollLog
 */
inline CRollLog& normal(CRollLog& log)
{
	log._debugtip = "";
	log._level = CRollLog::NORMAL_LOG;
	return log;
}

/**
 * ������־����logΪerror��Ϣ,�÷� rolllog << error
 * @see #tfc::base::CRollLog
 */
inline CRollLog& error(CRollLog& log)
{
	log._debugtip = "ERROR:";
	log._level = CRollLog::ERROR_LOG;
	return log;
}

/**
 * ������־����logΪnolog��Ϣ,�÷� rolllog << nolog
 * @see #tfc::base::CRollLog
 */
inline CRollLog& nolog(CRollLog& log)
{
	log._debugtip = "";
	log._level = CRollLog::NO_LOG;
	return log;
}

/**
 * ������־����һ�н���,�÷� rolllog << endl
 * @see #tfc::base::CRollLog
 */
inline CRollLog& endl(CRollLog& log)
{
	if(log.check_level()) log._os << std::endl;
	log._lock = false;
	return log;
}

/**
 * ������־����һ�н���,�÷� daylog << endl
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


