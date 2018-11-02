#ifndef _TFC_STAT_H_
#define _TFC_STAT_H_

#include "tfc_object.h"
#include "sys/time.h"
#include <string>
#include <map>


// the file has been modified by felixxie on 20091110
// if you want to get the file before,please open 20091110_bak_tfc_stat.h/.cpp in the same directory
using namespace std;

namespace tfc{ namespace stat{

	class CStat
	{
	public:	
		class CWatch
		{
		public:
			CWatch(CStat* stat, string iID)
			{
				assert(stat!=NULL);
				m_pStat = stat;
				m_strID = iID;
				gettimeofday(&m_stBegin, NULL);
			}
			~CWatch()
			{
				gettimeofday(&m_stEnd, NULL);
				m_pStat->IncCostStat(m_strID, m_stBegin, m_stEnd);
			}

		private:
			struct timeval m_stBegin;
			struct timeval m_stEnd;
			string m_strID;
			CStat* m_pStat;
		};
		
		CStat()
		{
			Reset();
		}

		virtual ~CStat(){}
		int Open(TLogPara &stLogPara);
		void SetLevel(int level);
		void Reset();
		virtual void Print();

		void SetStat(string eIdx, unsigned val)
		{
			map<string , long long>::iterator it;

			if((it = m_aiStatSlot.find(eIdx)) == m_aiStatSlot.end())
			{
				m_aiStatSlot.insert(pair<string , long long>(eIdx, 0));
			}

			it = m_aiStatSlot.find(eIdx);
			assert(it != m_aiStatSlot.end());

			it->second = val;
		}

		void IncStat(string eIdx)
		{
			map<string , long long>::iterator it;

			if((it = m_aiStatSlot.find(eIdx)) == m_aiStatSlot.end())
			{
				m_aiStatSlot.insert(pair<string , long long>(eIdx, 0));
			}

			it = m_aiStatSlot.find(eIdx);
			assert(it != m_aiStatSlot.end());

			it->second += 1;

		}

		void IncStat(string eIdx, unsigned val)
		{
			map<string , long long>::iterator it;

			if((it = m_aiStatSlot.find(eIdx)) == m_aiStatSlot.end())
			{
				m_aiStatSlot.insert(pair<string , long long>(eIdx, 0));
			}

			it = m_aiStatSlot.find(eIdx);
			assert(it != m_aiStatSlot.end());

			it->second += val;

		}
		long long GetStat(string eIdx)
		{
			map<string ,long long>::iterator it;
			if((it = m_aiStatSlot.find(eIdx)) == m_aiStatSlot.end())
			{
				return -1;
			}
			else
			{
				return it->second;
			}
		}

		long long GetStatValue(string eIdx)
		{
			map<string ,long long>::iterator it;
			if((it = m_aiStatSlot.find(eIdx)) == m_aiStatSlot.end())
			{
				return 0;
			}
			else
			{
				return it->second;
			}
		}
		
	protected:

		void IncCostStat(string id, struct timeval & tvBegin, struct timeval &tvEnd);		
		
		struct timeval GetCostStat(string id);

		TFCDebugLog m_StatFile;	
		static CStat * m_pInstance;
		map<string, long long> m_aiStatSlot;
		map<string, struct timeval> m_cmd_tv;	  
		friend class CWatch;
		
	};

}}

#endif

