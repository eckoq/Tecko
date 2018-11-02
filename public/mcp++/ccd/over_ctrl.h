#ifndef _OVER_CTRL_H_
#define _OVER_CTRL_H_

/*
 *  Copyright (c) parkyang 
 *
 *  Create date 2016-04-26
 */

#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define OVER_CTRL (OverCtrl::Instance())


class OverCtrl {

	public:
		typedef struct {
			unsigned long long _cpu_total_time;
			unsigned long long _cpu_used_time;
		} cpu_info;

		typedef struct {
			unsigned           _speed;
			unsigned long long _send_bytes;
			unsigned long long _recv_bytes;
			unsigned long long _send_packs;
			unsigned long long _recv_packs;
		} net_info;

		OverCtrl();
		virtual ~OverCtrl();

		inline unsigned short get_cores_count() 
		{
			return _cpu_cores;
		};

		void init(int pid, int use_ctrl, int thread_num = 1);

		/* get the cpu cores number */
		int get_cpu_cores();

		/* return value: bytes */
		inline long get_mem_total_used()
		{
			return _mem_total_used;
		};

		/* return value: bytes */
		inline long get_mem_free()
		{
			return _mem_freed;
		};

		/* get proc virtual memory used */
		inline long get_pv_mem_used()
		{
			return _self_vm_mem_used;
		};

		/* get proc phyisics memory */
		inline long get_proc_phy_mem_used()
		{
			return _self_phy_mem_used;
		};

		inline unsigned short get_mem_rate()
		{
			return _mem_rate;
		};

		/* get system info stat gap */
		inline unsigned get_collect_gap()
		{
			return _gap;
		};

		int get_network_flow(unsigned gap, std::map<std::string, net_info>& map_net_work);

		/* set collect info gap */
		inline void set_collect_gap(unsigned gap)
		{
			//if gap larger than 1000ms , error
			if(gap>1000) {
				return;
			}
			_gap = gap;
		};

		/* set cpu over load rate */
		int set_cpu_overload_rate(int rate)
		{
			if(rate>100||rate<0) {
				return -1;
			}
			_cpu_over_rate = rate;
			return 0;
		}

		/* set network flow over load rate */
		int set_network_over_flow_rate(int rate)
		{
			if(rate>100||rate<0) {
				return -1;
			}
			_net_work_over_rate = rate;
			return 0;
		}
			

		int start_thread();
		int is_over_load(int &rate);
		/* 
		 * cpuName:the cpu core name. Ex:cpu, cpu0, cpu1, cpu2
		 * gap:the time to caculate the cpu average used rate, can not be larger than 60 seconds
		 * and less than 1 seconds 
		 *
		 */
		int get_cpu_rate(const std::string &cpuName, unsigned gap);
        /* 
		 * gap:the time to caculate the cpu average used rate, can not be larger than 60 seconds
		 * and less than 1 seconds 
		 *
		 */
		int get_proc_cpu_rate(unsigned gap);


		static OverCtrl * Instance();

	protected:
    	void init_cpu();
		void init_network();
		void reset_net_info(const std::string &net_name, net_info *info, unsigned len);

        int  read_cpu_info();
		int  cal_overload_rate();
		int  read_network_info();
		int  read_proc_cpu_info();
		int get_netcard_speed(const std::string &name);

		const char* get_items(char* buffer,int ie);

		/*
		 * get the memory info
		 * param1:total memory used
		 * param2:total memory free
	     * param3:used rate
		 *
		 */
		int read_mem_info(unsigned long& mem_total_used,
				unsigned long& mem_freed,
				unsigned short& mem_use_rate);

	private:
	    int  _go; 		        /* thread running flag */
		int  _pid; 		        /* collect pid mem,cpu info */
		long _page_size; 		/* every second's cpu core used time and total time info  */
		int  _over_load_rate; 	/* overload rate */
		int _use_ctrl;


		unsigned         _gap;             		        /* collect cycle. unit: ms          */
		unsigned         _thread_num; 		            /* thread num in process            */
		unsigned        *_proc_now_cpu_array; 		    /* every second's process used time */
    	unsigned         _last_net_update_time; 	    /* network info last update time    */
        unsigned         _last_cpu_update_time; 	    /* cpu info last update time        */
		unsigned        *_proc_last_min_cpu_array;
		unsigned         _last_proc_cpu_update_time;    /* proc info last update time       */
		unsigned         _net_work_over_rate;           /* net work overflow percent        */
        unsigned         _cpu_over_rate;                /* cpu overload  percent            */

        unsigned short  _mem_rate;
		unsigned short  _cpu_cores;                     /* cpu core num                     */

        unsigned long   _mem_freed;
        unsigned long   _mem_total_used;                /* system memory info               */
		unsigned long   _self_vm_mem_used;  	        /* process memory info              */
		unsigned long   _self_phy_mem_used;

     	std::map<std::string, OverCtrl::cpu_info*> _map_cpu_rates;		        /* store every cpu core's info in this minute */
		std::map<std::string, OverCtrl::net_info*> _map_current_net_info; 		/* store eth info in this minute */
		std::map<std::string, OverCtrl::cpu_info*> _map_last_min_cpu_rates;		/* store every cpu core's info in last minute */
		std::map<std::string, OverCtrl::net_info*> _map_last_min_net_info;		/* store eth info in this minute */
		
		pthread_t _thread_id;

		cpu_info *_cpu_info_array;

		static void * over_ctrl_func(void *p);

};
#endif
