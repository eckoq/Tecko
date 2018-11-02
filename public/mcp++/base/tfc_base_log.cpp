
#include "tfc_base_log.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <fstream>

using namespace tfc::base;

log_open_fail::log_open_fail(const string & s):runtime_error(s) {}

CRollLog::CRollLog()
{
	_setlevel = DEBUG_LOG;
	_level = NORMAL_LOG;
	_buf_count = 0;
	_pid = getpid();

	_flags[0] = true;
	_flags[1] = false;
	_flags[2] = false;
	_flags[3] = true;

	_lock = false;

	time_format();
}

CRollLog::~CRollLog()
{
	if(_os.is_open())
	_os.close();
}

void CRollLog::Init(const string & sPreFix, const string & module, size_t maxsize, size_t maxnum)
	throw(log_open_fail)
{
	if(_os.is_open())	_os.close();
	_module = module;
	_filename = sPreFix;
	_max_log_size = maxsize;
	_max_log_num = maxnum;
	_buf_count = 0;
	_pid = getpid();

	_flags[0] = true;
	_flags[1] = false;
	_flags[2] = false;
	_flags[3] = true;

	_lock = false;

	_os.open((_filename+".log").c_str(),ios::app);
	if(!_os.is_open()) {
		throw log_open_fail(string("can not open file:")+_filename+".log");
	}
}

void CRollLog::set_titleflag(title_flag f)
{
	_flags[f] = true; 
	_pid = getpid(); 
	switch(_level) {
	case NO_LOG:
		_debugtip = "";
		break;
	case ERROR_LOG:
		_debugtip = "ERROR:";
		break;
	case NORMAL_LOG:
		_debugtip = "";
		break;
	case DEBUG_LOG:
		_debugtip = "DEBUG:";
		break;
	default:
		break;
	}
}

inline string CRollLog::cur_time()
{
	time_t tNow = time(0);
	struct tm curr;
	curr = *localtime(&tNow);
	char sTmp[1024];
	strftime(sTmp,sizeof(sTmp),_time_format.c_str(),&curr);
	return string(sTmp);
}

bool CRollLog::check_level()
{
	if(!_os.is_open()) {_os.open((_filename+".log").c_str(),ios::app);}
	if(!_os.is_open()) return false;

	_buf_count++;
	if(_buf_count % FLUSH_COUNT == 0) {
		_buf_count = 0;

		// close and roll file
		if(_os.tellp() > (int)_max_log_size) {
			// 2005-06-06, 多线程(进程)时会写乱,需要重新open之后判断
			if(_os.is_open())	_os.close();
			_os.open((_filename+".log").c_str(),ios::app);
			if(!_os.is_open()) return false;
			if(_os.tellp() > (int)_max_log_size) {
				char sTmp[16];
				if(_os.is_open())	_os.close();
				string from,to;
				// remove the last one
				sprintf(sTmp,"%d",_max_log_num-1);
				from = _filename+sTmp+".log";
				if (access(from.c_str(), F_OK) == 0) {
					if (remove(from.c_str()) < 0) {
						cerr << "remove " << from << " fail!!!" << std::endl;
						// return _level<=_setlevel;
					}
				}
				// rename the others
				for (int i = _max_log_num-2; i >= 0; i--) {
					sprintf(sTmp,"%d",i);
					if (i == 0) from = _filename+".log";
					else from = _filename+sTmp+".log";
					sprintf(sTmp,"%d",i+1);
					to = _filename+sTmp+".log";
					if (access(from.c_str(), F_OK) == 0) {
						if (rename(from.c_str(), to.c_str()) < 0) {
							cerr << "rename " << from << " --> " << to << " fail!!!" << std::endl;
						}
					}
				}
				_os.open((_filename+".log").c_str(),ios::app);
				if(!_os.is_open()) return false;
			}
		}
	}	

	if(_level<=_setlevel) {
		if(!_lock) {
			if(_flags[F_Time]){
				_os << cur_time() << " ";
			}
			if(_flags[F_Module]) {
				_os << _module << " ";
			}
			if(_flags[F_PID]) {
				_os << _pid << " ";
			}
			if(_flags[F_DEBUGTIP]) {
				_os << _debugtip << " ";
			}
			_lock = true;
		}
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
CDayLog::CDayLog()
{
	_flags[0] = true;
	_flags[1] = false;
	_flags[2] = false;

	_fd = -1;

	_lock = false;
	_last = 0;
	_pid = getpid();

	time_format();
}

CDayLog::~CDayLog()
{
	UnLock(); 
	if(_fd>0) {
		close(_fd);
		_fd = -1;
	}
}

void CDayLog::Init(const string& sPreFix,const string& module) throw(log_open_fail)
{
	_module = module;
	_filename = sPreFix;
	_pid = getpid();

	_fd = open((_filename+"_"+s_day(time(0))+".log").c_str(), O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE, 0666);
	if(_fd<0) {
		throw log_open_fail(string("can not open file:")+_filename+"_"+s_day(time(0))+".log");
	}
}

void CDayLog::set_titleflag(title_flag f)
{
	_flags[f] = true; 
	_pid = getpid(); 
}

inline string CDayLog::s_time(time_t t)
{
	struct tm curr;
	curr = *localtime(&t);
	char sTmp[1024];
	strftime(sTmp,sizeof(sTmp),_time_format.c_str(),&curr);
	return string(sTmp);
}

inline string CDayLog::s_day(time_t t)
{
	struct tm curr;
	curr = *localtime(&t);
	char sTmp[1024];
	strftime(sTmp,sizeof(sTmp),"%Y%m%d",&curr);
	return string(sTmp);
}

inline time_t CDayLog::t_day(time_t t)
{
	struct tm curr;
	curr = *localtime(&t);
	curr.tm_hour=0;
	curr.tm_min=0;
	curr.tm_sec=0;

	return mktime(&curr);
}

bool CDayLog::Lock()
{
	if(_fd<0) {
		_fd = open((_filename+"_"+s_day(time(0))+".log").c_str(),O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE);
	}

	if(_fd<0) {
		return false;
	}

	if(!_lock) {
		time_t tnow = time(0);
		if((tnow - _last) / 86400 > 0) {
//			if(_lock) {_os<<unlock;_lock = false;}
			if(_fd>0) close(_fd);
			_fd = open((_filename+"_"+s_day(time(0))+".log").c_str(),O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE);
			if(_fd<0) return false;
			_last = t_day(tnow);
		}

		//2005-05-09,在gcc3下<< lock编译不过,临时屏蔽
		//_os << lock; 
		char spacebuf[2] = {" "};
		if(_flags[F_Time]){
			write(_fd,s_time(tnow).c_str(),s_time(tnow).length());
			write(_fd,spacebuf,1);
		}
		if(_flags[F_Module]) {
			write(_fd,spacebuf,1);
		}
		if(_flags[F_PID]) {
			write(_fd,spacebuf,1);
		}
		_lock = true;
	}

	return true;
}

void CDayLog::UnLock()
{
	if(_lock) {
		//if(_os.is_open()) 
			//_os<<unlock; 
		_lock = false;
	}
}

CDayLog& CDayLog::operator<<(char n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		tmpbuf[0] = n; tmpbuf[1] = '\0';
		write(_fd,tmpbuf,1);
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(unsigned char n) 
{ 
	if(Lock()) {
		char tmpbuf[8];
		tmpbuf[0] = n; tmpbuf[1] = '\0';
		write(_fd,tmpbuf,1);
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(short n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%d",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}
CDayLog& CDayLog::operator<<(unsigned short n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%u",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(int n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%d",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(unsigned int n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%d",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}


CDayLog& CDayLog::operator<<(long n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%ld",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(unsigned long n)
{ 
	if(Lock()) {
		char tmpbuf[8];
		snprintf(tmpbuf,sizeof(tmpbuf),"%lu",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(long long n)
{ 
	if(Lock()) {
		char tmpbuf[16];
		snprintf(tmpbuf,sizeof(tmpbuf),"%lld",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(unsigned long long n)
{ 
	if(Lock()) {
		char tmpbuf[16];
		snprintf(tmpbuf,sizeof(tmpbuf),"%llu",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(double n)
{ 
	if(Lock()) {
		char tmpbuf[16]={0};
		snprintf(tmpbuf,sizeof(tmpbuf)-1,"%f",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(float n)
{ 
	if(Lock()) {
		char tmpbuf[16]={0};
		snprintf(tmpbuf,sizeof(tmpbuf)-1,"%f",n);
		write(_fd,tmpbuf,strlen(tmpbuf));
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(const string& s)
{ 
	if(Lock()) {
		write(_fd,s.c_str(),s.length());
	} 
	return *this;
}

CDayLog& CDayLog::operator<<(const char *cc)
{
	string s(cc);
	if(Lock()) {
		write(_fd,s.c_str(),s.length());
	} 
	return *this;
}


