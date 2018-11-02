
/*
 *  Copyright (c) parkyang 
 *
 *  Create date 2016-04-26
 */

#include <sys/time.h>
#include<stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>

typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned char      u8; 
typedef unsigned long long u64;

#include <linux/sockios.h>
#include <linux/ethtool.h>

#include "over_ctrl.h"

#define MEMTOTAL "MemTotal:"
#define MEMFREE  "MemFree:"

#define MAX_SECOND              (60)
#define ONE_MINUTE              (60)
#define SPEED_3500              (3500)
#define GET_DATA_GAP            (3)
#define CPU_OVER_LOAD_RATE      (80)
#define NET_WORK_OVER_LOAD_RATE (70)


static OverCtrl g_resStat;

OverCtrl * OverCtrl::Instance()
{
	return &g_resStat;	
}


OverCtrl::OverCtrl() 
{
	_go                        = 0;
    _use_ctrl                  = 0;
    _gap                       = 100;
	_cpu_cores                 = get_cpu_cores();
	_thread_id                 = 0;
	_over_load_rate            = 0;
  	_self_vm_mem_used          = 0;
	_self_phy_mem_used         = 0;
	_last_net_update_time      = 0;
    _last_cpu_update_time      = 0;
	_last_proc_cpu_update_time = 0;
	_cpu_over_rate             = CPU_OVER_LOAD_RATE;
	_net_work_over_rate        = NET_WORK_OVER_LOAD_RATE;
	_page_size = sysconf(_SC_PAGE_SIZE);
}

OverCtrl::~OverCtrl() 
{
	_go = 0;
	if(_thread_id)
	{
		pthread_join(_thread_id, NULL);
	}
	delete[] _cpu_info_array;
	_cpu_info_array = 0;
    delete[] _proc_now_cpu_array;
	_proc_now_cpu_array = 0;
    delete[] _proc_last_min_cpu_array;
	_proc_last_min_cpu_array = 0;
	std::map<std::string, OverCtrl::net_info*>::iterator iter = _map_current_net_info.begin();
	std::map<std::string, OverCtrl::net_info*>::iterator iter_end = _map_current_net_info.end();
	for(;iter!=iter_end;iter++)
	{
		delete [](iter->second);
		iter->second = 0;
	}

	iter = _map_last_min_net_info.begin();
	iter_end = _map_last_min_net_info.end();
	for(;iter!=iter_end;iter++)
	{
		delete [](iter->second);
		iter->second = 0;
	}
	_map_current_net_info.clear();
	_map_last_min_net_info.clear();
}


void OverCtrl::init(int pid, int use_ctrl, int iThreadNum) 
{
	_pid = pid;
      _use_ctrl = use_ctrl;
	_thread_num  = iThreadNum;
	init_network();
	init_cpu();
}


int OverCtrl::cal_overload_rate()
{
#define a (5)
#define b (1)
#define t (2)
#define m (10)
	static int iCount     = 0;    /* 连续判断为非过载的次数 */
	static int iOverCount = 0;    /* 连续判断为过载的次数 */
	std::map<std::string, OverCtrl::net_info> map_net_work;
	int iProcCpuRate = get_proc_cpu_rate(GET_DATA_GAP);
//	int iCpuRate     = get_cpu_rate(std::string("cpu"), GET_DATA_GAP);
	if(iProcCpuRate>(int)_cpu_over_rate) {
		iCount = 0;
		iOverCount++;
		if(_over_load_rate<20) { 
			_over_load_rate += a; 
		} else {
		   	_over_load_rate += t; 
		}

		//连续4次过载，增大起跳点
		if(iOverCount>4) {
			_over_load_rate += m;
		}
		_over_load_rate = _over_load_rate>100?100:_over_load_rate;
		return _over_load_rate;
	}
	get_network_flow(GET_DATA_GAP, map_net_work);
	std::map<std::string, OverCtrl::net_info>::iterator iter = map_net_work.begin();
	for(;iter!=map_net_work.end();iter++)
	{
		unsigned           speed      = iter->second._speed;
		unsigned long long send_flow  = iter->second._send_bytes/1000/1000*8/GET_DATA_GAP;
		unsigned long long recv_flow  = iter->second._recv_bytes/1000/1000*8/GET_DATA_GAP;
		if(send_flow>speed*_net_work_over_rate/100||recv_flow>speed*_net_work_over_rate/100) {
			iCount = 0;
			iOverCount++;
			if(_over_load_rate<20) {
				_over_load_rate += a; 
			} else {
				_over_load_rate += t; 
			}

			//连续4次过载，增大起跳点
			if(iOverCount>4) { 
				_over_load_rate += m;
			}
			_over_load_rate = _over_load_rate>100?100:_over_load_rate;
			return _over_load_rate;
		}
	}
	iCount++;
	iOverCount = 0;
	if(_over_load_rate>0) {
		if(iCount<5) {
		   	_over_load_rate -= b;  /*连续10次内判断为非过载则按1%递减*/
		} else {
			_over_load_rate -= m;  /*连续10次判断为非过载则按10%递减 */
		}
	}
	_over_load_rate = _over_load_rate>=0?_over_load_rate:0;
	return _over_load_rate;
}

int OverCtrl::is_over_load(int &rate)
{    rate = _over_load_rate;
      if(!_use_ctrl) {
        return false;
        }	
	return _over_load_rate>rand()%100?true:false;
}

void OverCtrl::init_cpu()
{
	char cpuName[32] = {0};

	/*
	 * store total cpu, every cpu core used time and free time in every second.
	 *so we need to all core's info and total cpu info to caculate the cpu rate
	 *in a minute 
     *
	 */
	_cpu_info_array            = new cpu_info[(_cpu_cores+1)*ONE_MINUTE*2];
	_proc_now_cpu_array        = new unsigned[ONE_MINUTE];
	_proc_last_min_cpu_array   = new unsigned[ONE_MINUTE];

	memset(_cpu_info_array,          0x00, sizeof(cpu_info)*(_cpu_cores+1)*ONE_MINUTE*2);
	memset(_proc_now_cpu_array,      0x00, sizeof(unsigned)*ONE_MINUTE);
	memset(_proc_last_min_cpu_array, 0x00, sizeof(unsigned)*ONE_MINUTE);

	/* total cpu info */
	_map_cpu_rates.insert(std::pair<std::string, OverCtrl::cpu_info*>(std::string("cpu"), _cpu_info_array));
	OverCtrl::cpu_info *lastMinInfoStatPosition = _cpu_info_array + (_cpu_cores + 1)*ONE_MINUTE;
	_map_last_min_cpu_rates.insert(std::pair<std::string, OverCtrl::cpu_info*>(std::string("cpu"), lastMinInfoStatPosition));

	for(int i=0;i<_cpu_cores;i++) 
	{
		sprintf(cpuName, "cpu%d", i);
		/*
		 *i start with 0, and the first 60 slot is used by total cpu info.
		 *we need to start with _cpu_info_array+60
		 */
		_map_cpu_rates.insert(std::pair<std::string, OverCtrl::cpu_info*>(std::string(cpuName), _cpu_info_array+(i+1)*ONE_MINUTE));
		_map_last_min_cpu_rates.insert(std::pair<std::string, OverCtrl::cpu_info*>(std::string(cpuName), lastMinInfoStatPosition+(i+1)*ONE_MINUTE));
	}
}


int OverCtrl::read_mem_info(unsigned long& mem_total, unsigned long& mem_freed, unsigned short& mem_used_rate)
{
	FILE *mem_fd = fopen("/proc/meminfo", "rb");

	if(mem_fd==NULL)
		return -1;

	char buf[1024] = {0};
	char * value   = NULL;

	/*move to start position to read the file */
	fseek(mem_fd, 0, SEEK_SET);
	while(fgets(buf, sizeof(buf), mem_fd)!=NULL)
	{
		if(strstr(buf, MEMTOTAL)) {
			strtok(buf, " ");
			char * p_mem_info = strtok(NULL, " ");
			if(p_mem_info) {
				mem_total = strtoul(p_mem_info, NULL, 10);
			} else {
				fclose(mem_fd);
				return -1;
			}
		} else if(strstr(buf, MEMFREE)) {
			strtok(buf, " ");
			char * p_mem_info = strtok(NULL, " ");
			if(p_mem_info) {
				mem_freed = strtoul(p_mem_info, NULL, 10);
			} else {
				fclose(mem_fd);
				return -1;
			}
		}
	}
	/* if memtotal equal to 0, error! */
	if(!mem_total) {
		fclose(mem_fd);
		return -1;
	}
	mem_used_rate = (unsigned)((100*(mem_total-mem_freed)/mem_total));
	fclose(mem_fd);

	char mem_info_file[1024] = {0};
	sprintf(mem_info_file, "/proc/%u/statm", getpid());
	FILE *pid_mem_fd = fopen(mem_info_file, "rb");

	if(pid_mem_fd==NULL) {
		return -1;
	}
	//move to the start position to read the pid mem info
	fseek(pid_mem_fd, 0, SEEK_SET);

	if(fgets(buf, sizeof(buf), pid_mem_fd)==NULL) {
		fclose(pid_mem_fd);
		return -1;
	}
	//virtual memory used
	value = strtok(buf, " ");
	if(value) {
		_self_vm_mem_used = atoi(value)*_page_size;
	}

	//physical memory used
	value = strtok(NULL, " ");
	if(value) {
		_self_phy_mem_used = atoi(value)*_page_size;
	}
	fclose(pid_mem_fd);
	return 0;
}

void OverCtrl::init_network()
{
	char          buf[2048];
	struct ifreq  ifr;
	struct ifconf ifc;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		printf("socket error\n");
		return;
	}   

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		printf("ioctl error\n");
		return;
	}   

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
	int count = 0;
	for (; it != end; ++it)
   	{
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			if (! (ifr.ifr_flags & IFF_LOOPBACK)) { /* don't count loopback */
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) 
				{
					count++ ;
					unsigned char * ptr = 0;
					ptr = (unsigned char  *)&ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
					if(strstr(ifr.ifr_name,"tun")==0) {
						std::string strIfrName(ifr.ifr_name);
						if(_map_current_net_info.find(strIfrName)==_map_current_net_info.end()) {
							net_info *info = new net_info[ONE_MINUTE];
							_map_current_net_info.insert(make_pair(strIfrName,info));
							reset_net_info(strIfrName, info, ONE_MINUTE);
						}
						if(_map_last_min_net_info.find(strIfrName)==_map_last_min_net_info.end()) {
							net_info *info = new net_info[ONE_MINUTE];
							_map_last_min_net_info.insert(make_pair(strIfrName,info));
							reset_net_info(strIfrName, info, ONE_MINUTE);
						}
					}
				}   
			}   
		} else {
			printf("get mac info error\n");
			return;
		}   
	}   
}

int OverCtrl::read_network_info()
{
	char   net_name[128] = {0};
	char   net_buf[2048] = {0};

	int    count         = 0;
	time_t now_time      = time(NULL);
	struct tm * tm_now   = gmtime(&now_time);

	//second can not be larger than 59
	if(tm_now->tm_sec>MAX_SECOND-1) {
	   	return -1; 
	}
	FILE *net_fd = fopen("/proc/net/dev", "rb");
	if(net_fd==NULL) {
	   	return -1; 
	}
	fseek(net_fd, 0, SEEK_SET);
	while(fgets(net_buf, sizeof(net_buf), net_fd)!=NULL)
	{
		count++;
        /* jump the header */
		if(count<=2) {
		   	continue; 
		} 
		unsigned long long send_bytes = 0, recv_bytes = 0, send_packs = 0, recv_packs = 0;
		unsigned nouse1, nouse2, nouse3, nouse4, nouse5, nouse6, nouse7, nouse8, nouse9, nouse10, nouse11, nouse12;
		int iRet = sscanf(net_buf,"\t%[^:]:%llu %llu %u %u %u %u %u %u %llu %llu %u %u %u %u %u %u",
				net_name,
				&recv_bytes,
				&recv_packs,
				&nouse1,
				&nouse2,
				&nouse3,
				&nouse4,
				&nouse5,
				&nouse6,
				&send_bytes,
				&send_packs,
				&nouse7,
				&nouse8,
				&nouse9,
				&nouse10,
				&nouse11,
				&nouse12);

		if(iRet<0) {
			fprintf(stderr, "read proc utime stime cutime cstime error\n");
			fclose(net_fd);
			return -1;
		}
		std::string str_net_name(net_name);
		std::map<std::string, OverCtrl::net_info*>::iterator itr = _map_current_net_info.find(str_net_name);
		if(itr!=_map_current_net_info.end()) {
			(itr->second)[tm_now->tm_sec]._send_bytes = send_bytes;
			(itr->second)[tm_now->tm_sec]._recv_bytes = recv_bytes;
			(itr->second)[tm_now->tm_sec]._send_packs = send_packs;
			(itr->second)[tm_now->tm_sec]._recv_packs = recv_packs;
			std::map<std::string, OverCtrl::net_info*>::iterator itr_last = _map_last_min_net_info.find(str_net_name);

			/* import the current minute's cpu info map to last minute's info map */
			if((_last_net_update_time!=(MAX_SECOND-1))
				&&(tm_now->tm_sec==(MAX_SECOND-1))
				&&itr_last!=_map_last_min_net_info.end()) {
				for(int i=0;i<MAX_SECOND;i++)
				{
					(itr_last->second)[tm_now->tm_sec]._send_bytes    = (itr->second)[tm_now->tm_sec]._send_bytes;
					(itr_last->second)[tm_now->tm_sec]._recv_bytes    = (itr->second)[tm_now->tm_sec]._recv_bytes;
					(itr_last->second)[tm_now->tm_sec]._send_packs    = (itr->second)[tm_now->tm_sec]._send_packs;
					(itr_last->second)[tm_now->tm_sec]._recv_packs    = (itr->second)[tm_now->tm_sec]._recv_packs;
				}/* end of max second */
			}/* end of if */
		}/* end of if */
	}/* end of while */
	_last_net_update_time = tm_now->tm_sec;
	fclose(net_fd);
	return 0;

}

void OverCtrl::reset_net_info(const std::string &net_name, net_info *info, unsigned len)
{
	if(!info||len<=0) { 
		return; 
	}

    int net_speed = get_netcard_speed(net_name);

	for(unsigned i=0;i<len;i++)
	{
		info[i]._speed       = net_speed;
		info[i]._send_bytes  = 0;
		info[i]._recv_bytes  = 0;
		info[i]._send_packs  = 0;
		info[i]._recv_packs  = 0;
	}
}


int OverCtrl::get_netcard_speed(const std::string &name)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sockfd < 0) {   
		return -1; 
	}   

	int    speed = SPEED_1000;
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	memset(&ifr, 0, sizeof(ifr));
	memset(&ecmd, 0, sizeof(ecmd));
	sprintf(ifr.ifr_name, name.c_str()); 
	ecmd.cmd     = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;

	if (ioctl(sockfd, SIOCETHTOOL, &ifr) == 0) { 
		if(SPEED_10000==ecmd.speed) {
			speed = SPEED_3500;
		}
		speed = ecmd.speed; 
	} else {   
		speed = SPEED_1000;
	}   
	close(sockfd);
	return speed;
}

int OverCtrl::get_cpu_cores()
{
	FILE * pFile = fopen("/proc/cpuinfo", "rb");
	if(pFile==NULL) {
		return -1;
	}

	fseek(pFile, 0, SEEK_END);
	int iFileLen   = 1024 * 1024;
	int offset     = 0;
	int iCpuCount  = 0;
	char * buf     = new char[iFileLen];

	iFileLen       = fread(buf, sizeof(char), iFileLen, pFile);
	char *position = 0;
	position = buf - 1;

	while(position != NULL)	
	{   
		position = strstr(position + 1, "processor");
		if(position != NULL) {
			offset += position - buf;
			iCpuCount++;
		}
		if(offset > 1024*100) {
			break;
		}
	}
	delete []buf;
	return iCpuCount;
}


int OverCtrl::read_cpu_info()
{
	time_t now_time    = 0;
	struct tm * tm_now = NULL;
	now_time           = time(NULL);
	tm_now             = gmtime(&now_time);

	//second can not be larger than 59
	if(tm_now->tm_sec>MAX_SECOND-1) {
	   	return -1; 
	}
	FILE *cpu_fd = fopen("/proc/stat", "rb");
	if(cpu_fd==NULL) {
	   	return -1; 
	}

	fseek(cpu_fd, 0, SEEK_SET);

	/* count the lines from /proc/cpuinfo , has fixed form */
	int lineCount  = 0;
	char buf[1024] = {0};

	/* tmp data for every section of cpuinfo */
	char *info     = NULL;
	char *cpu_name = NULL;
	unsigned long long user = 0, system = 0, nice = 0, idle = 0, iowait =0, irq =0, softirq = 0;

	//read every raw info for every cpu
	while((fgets(buf, sizeof(buf), cpu_fd)!=NULL)&&(lineCount<=_cpu_cores+1))
	{
		lineCount++;
#ifdef _DEBUG_STAT_
		fprintf(stderr, "[%s] line:%s\n", __FUNCTION__, buf);
#endif
		//cpu name .ex :cpu ,cpu0, cpu1
		cpu_name = info = strtok(buf, " ");
		if(info) {
    		//read every column info for every cpu
			int column = 1;
			while(((info = strtok(NULL, " "))!=NULL)&&(column<=8))
			{
#ifdef _DEBUG_STAT_
				fprintf(stderr, "[%s] every info:%s\n", __FUNCTION__, info);
#endif
				unsigned long long num = strtoull(info, NULL, 10);
				switch(column) {
					case 1:
						user = num;
						break;
					case 2:
						nice = num;
						break;
					case 3:
						system = num;
						break;
					case 4:
						idle = num;
						break;
					case 5:
						iowait = num;
						break;
					case 6:
						irq = num;
						break;
					case 7:
						softirq = num;
						break;
					default:
						break;
				}//end switch
				column++;
			}//end while info==NULL

			std::map<std::string, OverCtrl::cpu_info*>::iterator itr; 
			itr = _map_cpu_rates.find(std::string(cpu_name));
			OverCtrl::cpu_info *info = NULL;

			if(itr!=_map_cpu_rates.end()) {
				info = itr->second;
				/* when acquire the cpu info, assignment to corresponding second */
				info[tm_now->tm_sec]._cpu_total_time = user + nice + system + idle + iowait + irq + softirq;
				info[tm_now->tm_sec]._cpu_used_time = info[tm_now->tm_sec]._cpu_total_time - iowait - idle;
			}
			memset(buf, 0x00, sizeof(buf));
		} //end if(info)
	}//end while

	/* import the current minute's cpu info map to last minute's info map */
	if((_last_cpu_update_time!=(MAX_SECOND-1))&&(tm_now->tm_sec==(MAX_SECOND-1))) {
		std::map<std::string, OverCtrl::cpu_info*>::iterator itr    = _map_cpu_rates.begin();	
		std::map<std::string, OverCtrl::cpu_info*>::iterator result = _map_last_min_cpu_rates.begin();

		for(;itr!=_map_cpu_rates.end();itr++) {
			result = _map_last_min_cpu_rates.find(itr->first);
			if(result==_map_last_min_cpu_rates.end()) {
			   	continue;
		   	}
			for(int i=0;i<MAX_SECOND;i++) {
				(result->second)[i] = (itr->second)[i];
			}
		}
	}
	_last_cpu_update_time = tm_now->tm_sec;
	fclose(cpu_fd);
	return 0;
}

int OverCtrl::read_proc_cpu_info()
{
	time_t     now_time = time(NULL);
	struct tm *tm_now   = gmtime(&now_time);

	/* second can not be larger than 59 */
	if(tm_now->tm_sec>MAX_SECOND-1) { 
		return -1; 
	}
	char cpuInfoFile[128] = {0};
	char cpuInfoBuf[2048] = {0};
	sprintf(cpuInfoFile, "/proc/%u/stat", _pid);
	FILE *cpu_fd = fopen(cpuInfoFile, "rb");
	if(cpu_fd==NULL) { 
		return -1; 
	}
	fseek(cpu_fd, 0, SEEK_SET);
	char *q = fgets(cpuInfoBuf, sizeof(cpuInfoBuf), cpu_fd);
	if(!q) {
	   	fprintf(stderr, "read proc cpu info err\n"); 
		fclose(cpu_fd); 
		return -1;
	}
#define PROCESS_ITEM (14)
	const char *p = get_items(cpuInfoBuf, PROCESS_ITEM);
	if(!p) {
		fprintf(stderr, "get item error\n");
		fclose(cpu_fd);
		return -1;
	}
	unsigned utime = 0, stime = 0, cutime = 0, cstime = 0;
	int iRet = sscanf(p,"%u %u %u %u",&utime,&stime,&cutime,&cstime);
	if(iRet<0) {
		fprintf(stderr, "read proc utime stime cutime cstime error\n");
		fclose(cpu_fd);
		return -1;
	}
	_proc_now_cpu_array[tm_now->tm_sec] = utime + stime + cutime + cstime;

	/* import the current minute's cpu info map to last minute's info map */
	if((_last_proc_cpu_update_time!=(MAX_SECOND-1))
		&&(tm_now->tm_sec==(MAX_SECOND-1))) {
		for(int i=0;i<MAX_SECOND;i++)
		{
			_proc_last_min_cpu_array[i] = _proc_now_cpu_array[i];
		}
	}
	_last_proc_cpu_update_time = tm_now->tm_sec;
	fclose(cpu_fd);
	return 0;
}


int OverCtrl::get_network_flow(unsigned gap, std::map<std::string, net_info>& setNetwork)
{
	if(gap>MAX_SECOND||gap<1) { 
		return -1;
   	}
	std::map<std::string, OverCtrl::net_info*>::iterator iter_now = _map_current_net_info.begin();
	for(;iter_now!=_map_current_net_info.end();iter_now++)
	{
		if(_last_net_update_time>=gap) {
			net_info info;
			info._speed = (iter_now->second)[_last_net_update_time]._speed;
			if((iter_now->second)[_last_net_update_time]._send_bytes!=0
				&&(iter_now->second)[_last_net_update_time-gap]._send_bytes!=0) {
				info._send_bytes = (iter_now->second)[_last_net_update_time]._send_bytes - (iter_now->second)[_last_net_update_time-gap]._send_bytes;
			} else {
				info._send_bytes = 0;
			}

			if((iter_now->second)[_last_net_update_time]._recv_bytes!=0
				&&(iter_now->second)[_last_net_update_time-gap]._recv_bytes!=0) {
				info._recv_bytes = (iter_now->second)[_last_net_update_time]._recv_bytes - (iter_now->second)[_last_net_update_time-gap]._recv_bytes;
			} else {
				info._recv_bytes = 0;
			}
			if((iter_now->second)[_last_net_update_time]._send_packs!=0
				&&(iter_now->second)[_last_net_update_time-gap]._send_packs!=0) {
				info._send_packs = (iter_now->second)[_last_net_update_time]._send_packs - (iter_now->second)[_last_net_update_time-gap]._send_packs;
			} else {
				info._send_packs = 0;
			}
			if((iter_now->second)[_last_net_update_time]._recv_packs!=0
				&&(iter_now->second)[_last_net_update_time-gap]._recv_packs!=0) {
				info._recv_packs = (iter_now->second)[_last_net_update_time]._recv_packs - (iter_now->second)[_last_net_update_time-gap]._recv_packs;
			} else {
				info._recv_packs = 0;
			}
			setNetwork.insert(make_pair(iter_now->first, info));
		} else {
			std::map<std::string, OverCtrl::net_info*>::iterator iter_last = _map_last_min_net_info.find(iter_now->first);
			if(iter_last==_map_last_min_net_info.end()) {
			   	continue; 
			}
			unsigned index = MAX_SECOND - (gap - _last_net_update_time) - 1;
			if(index>MAX_SECOND-1) { 
				continue; 
			}
			net_info info;
			info._speed = (iter_now->second)[_last_net_update_time]._speed;
			if((iter_now->second)[_last_net_update_time]._send_bytes!=0
				&&(iter_last->second)[index]._send_bytes!=0) {
				info._send_bytes = (iter_now->second)[_last_net_update_time]._send_bytes - (iter_last->second)[index]._send_bytes;
			} else {
				info._send_bytes = 0;
			}

			if((iter_now->second)[_last_net_update_time]._recv_bytes!=0
				&&(iter_last->second)[index]._recv_bytes!=0) {
				info._recv_bytes = (iter_now->second)[_last_net_update_time]._recv_bytes - (iter_last->second)[index]._recv_bytes;
			} else {
				info._recv_bytes = 0;
			}
			if((iter_now->second)[_last_net_update_time]._send_packs!=0
				&&(iter_last->second)[index]._send_packs!=0) {
				info._send_packs = (iter_now->second)[_last_net_update_time]._send_packs - (iter_last->second)[index]._send_packs;
			} else {
				info._send_packs = 0;
			}
			if((iter_now->second)[_last_net_update_time]._send_packs!=0
				&&(iter_last->second)[index]._send_packs!=0) {
			 	info._recv_packs = (iter_now->second)[_last_net_update_time]._recv_packs - (iter_last->second)[index]._recv_packs;
			} else {
				info._recv_packs = 0;
			}
			setNetwork.insert(make_pair(iter_now->first, info));
		}
	}
	return 0;
}

int OverCtrl::get_proc_cpu_rate(unsigned gap)
{
	if(gap>MAX_SECOND||gap<1) {
		return -1;
	}
	unsigned lastUpdate = _last_proc_cpu_update_time>_last_cpu_update_time?_last_cpu_update_time:_last_proc_cpu_update_time;
	std::map<std::string, OverCtrl::cpu_info*>::iterator itr = _map_cpu_rates.find("cpu");
	if(itr==_map_cpu_rates.end()) { 
		return -1; 
	}
	if(lastUpdate>=gap) {
		int total_gap = itr->second[lastUpdate]._cpu_total_time-itr->second[lastUpdate-gap]._cpu_total_time;
		int proc_gap  = _proc_now_cpu_array[lastUpdate] - _proc_now_cpu_array[lastUpdate - gap];
		/* guarantee the dividend can not be zero */
		total_gap = total_gap?total_gap:1;

		if(total_gap<=0||proc_gap<=0) {
			return -1;
		}
		return (int)(proc_gap*100/total_gap*_cpu_cores);
	} else {
		std::map<std::string, OverCtrl::cpu_info*>::iterator last_itr = _map_last_min_cpu_rates.find("cpu");
		if(last_itr==_map_last_min_cpu_rates.end()) { 
			return -1; 
		}
		unsigned index = MAX_SECOND - (gap - lastUpdate) - 1;
		if(index>MAX_SECOND-1) { 
			return -1;
		}
		if(last_itr->second[index]._cpu_total_time==0
		   ||last_itr->second[index]._cpu_used_time==0) { 
			return -1; 
		}
		int total_gap = itr->second[lastUpdate]._cpu_total_time - last_itr->second[index]._cpu_total_time;
		int proc_gap  = _proc_now_cpu_array[lastUpdate] - _proc_last_min_cpu_array[index];
		if(total_gap<=0||proc_gap<=0) {
			return -1;
		}
		unsigned uiFactor = _thread_num>_cpu_cores?_cpu_cores:_thread_num;
		return (int)(proc_gap*100*_cpu_cores/uiFactor/total_gap);
	}
}


int OverCtrl::get_cpu_rate(const std::string &cpu_name, unsigned gap)
{
	if(gap>MAX_SECOND||gap<1) {
		fprintf(stderr, "param gap is error!\n");
		return -1;
	}
	std::map<std::string, OverCtrl::cpu_info*>::iterator itr = _map_cpu_rates.find(cpu_name);
	if(itr==_map_cpu_rates.end()) {
		return -1;
	}
	if(_last_cpu_update_time>=gap) {
		unsigned total_gap = itr->second[_last_cpu_update_time]._cpu_total_time-itr->second[_last_cpu_update_time-gap]._cpu_total_time;

		/* guarantee the dividend can not be zero */
		total_gap = total_gap?total_gap:1;
		return (int)(itr->second[_last_cpu_update_time]._cpu_used_time-itr->second[_last_cpu_update_time-gap]._cpu_used_time)*100/total_gap;
	} else {
		std::map<std::string, OverCtrl::cpu_info*>::iterator last_itr = _map_last_min_cpu_rates.find(cpu_name);
		if(last_itr==_map_last_min_cpu_rates.end()) { 
			return -1; 
		}
		unsigned index = MAX_SECOND - (gap - _last_cpu_update_time) - 1;
		if(index>MAX_SECOND-1) { 
			return -1;	
		}
		if(last_itr->second[index]._cpu_total_time==0
		   ||last_itr->second[index]._cpu_used_time==0) { 
			return -1; 
		}
		int total_gap = itr->second[_last_cpu_update_time]._cpu_total_time - last_itr->second[index]._cpu_total_time;
		int used_gap  = itr->second[_last_cpu_update_time]._cpu_used_time - last_itr->second[index]._cpu_used_time;
		if(total_gap<=0||used_gap<=0) { 
			return -1; 
		}
		return (int)(used_gap*100/total_gap);
	}
}



void * OverCtrl::over_ctrl_func(void *p)
{
	unsigned uiUpdateTime = 0;
	struct timeval delay;
	struct timeval tvGap;
	
	while(OVER_CTRL->_go)
	{
		gettimeofday(&tvGap, 0);

		if(tvGap.tv_sec%GET_DATA_GAP==0&&uiUpdateTime!=tvGap.tv_sec) { 
			OVER_CTRL->cal_overload_rate(); 
			uiUpdateTime = tvGap.tv_sec;
		}
		//OVER_CTRL->read_mem_info(OVER_CTRL->_mem_total_used, OVER_CTRL->_mem_freed, OVER_CTRL->_mem_rate);
		OVER_CTRL->read_cpu_info();
		OVER_CTRL->read_proc_cpu_info();
		OVER_CTRL->read_network_info();
		delay.tv_sec = 0;
		delay.tv_usec = OVER_CTRL->_gap*1000;
		select(0, NULL, NULL, NULL, &delay);
	}
	return (void*)0;
}

int OverCtrl::start_thread()
{
	if(_go) { 
		return 0; 
	}
	/* thread work normally, set flag _go equal to 1 */
	_go = 1;
	int ret = pthread_create(&(_thread_id), NULL, OverCtrl::over_ctrl_func, NULL);
	if(ret!=0) {
		_go = 0;
		return -1;
	}
	/* to release the system resource when the thread terminate */
	return 0;
}

const char* OverCtrl::get_items(char* buffer,int ie)
{
	if(!buffer) { 
		return 0; 
	} 
	char* p = buffer;
	int len = strlen(buffer);
	int count = 0;
	if (1 == ie || ie < 1) {
		return p;
	}
	int i;

	for (i=0; i<len; i++)
	{
		if (' ' == *p) {
			count++;
			if (count == ie-1) {
				p++;
				break;
			}
		}
		p++;
	}
	return p;
}
