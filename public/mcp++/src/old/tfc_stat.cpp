
#include <sys/time.h>
#include "tfc_stat.h"

using namespace tfc::stat;

// the file has been modified by felixxie on 20091110
// if you want to get the file before,please open 20091110_bak_tfc_stat.h/.cpp in the same directory

int CStat::Open(TLogPara &stLogPara)
{
	return m_StatFile.open(stLogPara.log_level_, stLogPara.log_type_, 
		stLogPara.path_, stLogPara.name_prefix_, 
		stLogPara.max_file_size_, stLogPara.max_file_no_);	
}

void CStat::SetLevel(int level)
{
	m_StatFile.log_level(level);

}
void CStat::Reset()
{
	m_aiStatSlot.clear();
	m_cmd_tv.clear();
}

void CStat::IncCostStat(string cmd, struct timeval & tvBegin, struct timeval &tvEnd)
{
	map<string , struct timeval>::iterator it;

	if((it = m_cmd_tv.find(cmd)) == m_cmd_tv.end())
	{
		const struct timeval tv={0,0};
		m_cmd_tv.insert(pair<string , struct timeval>(cmd, tv));
	}

	it = m_cmd_tv.find(cmd);
	assert(it != m_cmd_tv.end());

	struct timeval &cur_tv = it->second;

	if(tvBegin.tv_usec > (tvEnd.tv_usec + cur_tv.tv_usec))
	{
		cur_tv.tv_usec += tvEnd.tv_usec  + 1000000 - tvBegin.tv_usec;
		cur_tv.tv_sec += tvEnd.tv_sec -1 - tvBegin.tv_sec;
	}
	else
	{
		long usec;
		usec = cur_tv.tv_usec + tvEnd.tv_usec - tvBegin.tv_usec;
		cur_tv.tv_usec = usec % 1000000;
		cur_tv.tv_sec += tvEnd.tv_sec - tvBegin.tv_sec + usec / 1000000;
	}
	// ´ÎÊý¼Ó1
	IncStat(cmd);
}

struct timeval CStat::GetCostStat(string cmd)
{
	map<string , struct timeval>::iterator it;
	if((it = m_cmd_tv.find(cmd)) == m_cmd_tv.end())
	{
		const struct timeval tv={0,0};
		return tv;
	}
	else
	{
		return it->second;
	}
}

void CStat::Print()
{
	map<string , long long>::iterator itSlot;
	map<string , struct timeval>::iterator itTime;

	for (itSlot = m_aiStatSlot.begin(); itSlot!=m_aiStatSlot.end(); itSlot++)
	{
		string cmd = itSlot->first;
		long long count = itSlot->second;
		if ((itTime = m_cmd_tv.find(cmd))!= m_cmd_tv.end())
		{
			assert(count!=0);
			struct timeval total_tv  = itTime->second;
			long avge = (1000000 * total_tv.tv_sec + total_tv.tv_usec) / count;
			m_StatFile.log_p_no_time(LOG_NORMAL, "Op: %s : Count %lld, Cost %d.%06d, Ave %d(usec)\n" ,
				cmd.c_str(), count, total_tv.tv_sec, total_tv.tv_usec, avge);
		}
	}

	for (itSlot = m_aiStatSlot.begin(); itSlot!=m_aiStatSlot.end(); itSlot++)
	{
		string cmd = itSlot->first;
		long long count = itSlot->second;
		if ((itTime = m_cmd_tv.find(cmd))== m_cmd_tv.end())
		{
			m_StatFile.log_p_no_time(LOG_NORMAL, "Op: %s : Count %lld \n" ,
				cmd.c_str(), count);
		}
	}
}

