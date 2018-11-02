#ifndef _STAT_MANAGER_H_
#define _STAT_MANAGER_H_
    
#include <string>
#include <map>
#include <vector>
#include "tfc_base_fast_timer.h"
    
namespace tfc{namespace cache
{
    typedef struct tagServerStat
    {
        unsigned ip;
        unsigned port;
        unsigned to_cnt;
        unsigned msg_cnt;
    }ServerStat;

    class StatMgr
    {
    public:
        static StatMgr* instance();
        static void release();
        
        void update_stat(std::string& server_name, unsigned ip, unsigned short port, bool is_timeout = false);
        void check_stat(struct timeval & cur_time)
        {
            _timer_queue.check_expire(cur_time);
        }

        void report();
        
        int stat_init(unsigned msg_seq, std::string& server_name, unsigned ip, unsigned short port);
        void stat_fini(unsigned msg_seq);

    private:
        std::map<std::string, std::vector<ServerStat> > _servers;
        tfc::base::CFastTimerQueue _timer_queue;
        static StatMgr* _stat_mgr;
    };

    class MsgTracker : public tfc::base::CFastTimerInfo
    {
    public:
        MsgTracker(std::string& server_name, unsigned ip, unsigned short port, unsigned msg_seq)
            :_ip(ip),_port(port)
        {
            _server_name.assign(server_name);
            ret_msg_seq = msg_seq;
        }
        ~MsgTracker(){}

        inline unsigned msg_seq()
        {
            return ret_msg_seq;
        }

		inline void on_expire()
        {
            StatMgr::instance()->update_stat(_server_name, _ip, _port, true);
        }

        friend class StatMgr;
        
    protected:
        std::string     _server_name;
        unsigned        _ip;
        unsigned short  _port;
    };

}}

#endif //_STAT_MANAGER_H_
///:~
