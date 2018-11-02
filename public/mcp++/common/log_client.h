/*
 * log_client.h:      log client client class for MCP++.
 * Date:              2013-03-06
 */

#ifndef __LOG_CLIENT_H__
#define __LOG_CLIENT_H__

#include "tfc_ipc_sv.hpp"
#include "tfc_base_config_file.h"

namespace tools
{
    typedef struct tagLogMapHeader
    {
        int verify_code;
        unsigned short version;
        unsigned char  hdr_len;
        unsigned char  log_level;
        unsigned log_line_num;
        unsigned cur_line;
        unsigned short log_line_len;
        unsigned stat_len;
        unsigned short stat_gap;
        unsigned short rsvd;

        uint32_t ip; // from ccd
        uint32_t port_count;
        uint32_t port[10]; // from ccd
        char so_md5[32]; // from mcd
        char mcppp_version[32]; // from ccd
        char mcppp_compiling_date[32]; // from ccd

    }LogMapHdr;
    #define LOG_MAP_HDR_LEN     (sizeof(LogMapHdr))

    class CLogClient
    {
        public:
            CLogClient() : _inited(false),_hdr_info(NULL){}         // Construct but not initizalie. Call init() before use log client.
            ~CLogClient();                          // Destructor.
            int init(const std::string& conf_file); // Initialize the log client.
            bool is_inited();                       // Whether the log map has been initialized.

            int  change_log_level(unsigned char level);  // switch to a new log level
            int  change_stat_gap(unsigned short stat_gap);  // change statistics period
            unsigned short get_stat_gap(){return _inited ? _hdr_info->stat_gap: 0;} // get statistics period
            void log_seek(int offset, int whence);                  // similar to fseek
            int  read_log(unsigned& event, struct timeval& timestamp, void*& log_info);
            int  write_log(unsigned event, struct timeval* timestamp, void* log_info);

            int read_stat(unsigned type, void*& stat_info);
            int write_stat(unsigned type, void* stat_info);

            int read_remote_info(void*& remote_info, uint32_t *data_len, uint32_t *item_num);
            int write_remote_info(void* remote_info, uint32_t data_len, uint32_t item_num);

            void DebugShmKey();
            bool SetIpWithInnerIp();
            void GetIp(uint32_t *ip);
            bool SetCCDPort(const tfc::base::CFileConfig& page);
            void GetCCDPort(uint32_t *port_buf, uint32_t port_buf_len, uint32_t *port_num, uint32_t *mini_port);
            bool SetSoMd5(const char *buf, uint32_t len);
            void GetMd5(std::string *md5);
            bool SetMcpppVersion(const char *buf, uint32_t len);
            void GetMcpppVersion(char *buf, uint32_t len);
            bool SetMcpppCompilingDate(const char *buf, uint32_t len);
            void GetMcpppCompilingDate(char *buf, uint32_t len);
            static bool GetLocalIp(uint32_t *ip);

        private:
            tfc::ptr<tfc::ipc::CShm> _shm;
            bool       _inited;                     // Whether the log map has been initialized.
            LogMapHdr  _log_hdr;                    // variables that will not change frequently
            unsigned   _read_line;
            char*      _log_blk;
            char*      _stat_blk;
            char*      _remote_info_blk;
            LogMapHdr* _hdr_info;                   // variables that will change frequently
            key_t _shm_key;
    };
}

#endif //__LOG_CLIENT_H__
///:~
