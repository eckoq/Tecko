/*
 * log_client.cpp:      log client class for MCP++.
 * Author:              saintvliu
 * Created date:        2013-03-06
 */
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "log_client.h"
#include "tfc_base_str.h"
#include "tfc_base_config_file.h"
#include "tfc_md5.h"
#include "log_type.h"
#include "stat.h"

using namespace std;
using namespace tfc::ipc;
using namespace tfc::base;
using namespace tools;

#define FTOK_PROJ_ID        0x12345678
#define DEF_LOG_BUF_SIZE    (1<<23)     // 8MB
#define DEF_LOG_LINE_LEN    (1<<6)      // 64B
#define DEF_STAT_NUM        (1<<6)      // 64
#define DEF_REMOTE_INFO_NUM (1 << 10)    // 1024
#define DEF_STAT_GAP        60          // 60s
#define LOG_VERSION         0x0001

#define LOG_LINE_HLEN       (sizeof(unsigned) + sizeof(struct timeval))

#define NET_STAT_BLK_SIZE   (DEF_STAT_NUM * sizeof(CCDStatInfo))
#define MCD_STAT_BLK_SIZE   (DEF_STAT_NUM * sizeof(MCDStatInfo))
#define DEF_STAT_LEN        (MCD_STAT_BLK_SIZE + NET_STAT_BLK_SIZE)
#define REMOTE_INFO_BLK_SIZE (2 * sizeof(uint32_t) + DEF_REMOTE_INFO_NUM * sizeof(RemoteInfo))

const unsigned DEF_LOG_MAP_SIZE = LOG_MAP_HDR_LEN + DEF_LOG_BUF_SIZE + DEF_STAT_LEN + REMOTE_INFO_BLK_SIZE;

static int check_log_header(LogMapHdr* map_hdr, key_t shm_key)
{
    if (shm_key != map_hdr->verify_code)
    {
        fprintf(stderr, "check log header failed: verification code does not match.\n");
        return -1;
    }

    if (LOG_VERSION == map_hdr->version)
    {
        if ((LOG_MAP_HDR_LEN != map_hdr->hdr_len) ||
            (DEF_LOG_LINE_LEN != map_hdr->log_line_len) ||
            (DEF_STAT_LEN != map_hdr->stat_len) ||
            ((DEF_LOG_BUF_SIZE / DEF_LOG_LINE_LEN) != map_hdr->log_line_num))
        {
            fprintf(stderr, "check log header (version %d) failed.\n", LOG_VERSION);
            return -1;
        }

        return 0;
    }

    return -1;
}

/*
 * ~CLogClient():       Destructor, release shm resource.
 */
CLogClient::~CLogClient()
{
    if ( _inited ) {
        _inited = false;
    }
}

/*
 * Init():              Initialize the log client. Must call this before read/write logs.
 * @conf_name:          log configure file path.
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::init(const string& conf_name)
{
    if ( !_inited )
    {
        key_t shm_key = -1;
        CFileConfig page;
        unsigned log_client_log_level = 1;
        try
        {
            page.Init(conf_name);
            try {
                log_client_log_level = from_str<unsigned>(page["root\\log_client\\log_level"]);
                fprintf(stderr, "use user defined log level: %d.\n", log_client_log_level);
            } catch(...) {
                log_client_log_level = 1;
                fprintf(stderr, "use log level: %d.\n", log_client_log_level);
            }
            shm_key = (key_t)from_str<int>(page["root\\log_client\\shm_key"]);
        }
        catch(...)
        {
            char tmp_path[PATH_MAX];
            if (NULL == realpath(conf_name.c_str(),tmp_path))
            {
                printf("get realpath for conf_file failed: %m\n");
                return -1;
            }

            MD5_CTX md5_ctx;
            MD5Init(&md5_ctx);
            MD5Update(&md5_ctx,(unsigned char*)tmp_path,strlen(tmp_path));
            unsigned char key[16] = {0};
            MD5Final(key, &md5_ctx);

            shm_key = *(key_t*)key;
            if (-1 == shm_key)
            {
                shm_key = *(key_t*)(key + 4);
                if (-1 == shm_key)
                {
                    fprintf(stderr, "get shm_key for log_client failed\n");
                    return -1;
                }
            }
            // printf("log client key for \"%s\" is 0x%08x\n", tmp_path, shm_key);

            if (shm_key < 0)
                shm_key = 0 - shm_key;
        }

        try
        {
            try
            {
                _shm = CShm::create_only(shm_key, DEF_LOG_MAP_SIZE);
                memset(_shm->memory(), 0, DEF_LOG_MAP_SIZE);
                _hdr_info = (LogMapHdr*)_shm->memory();
                _hdr_info->verify_code  = shm_key;
                _hdr_info->version      = LOG_VERSION;
                _hdr_info->hdr_len      = LOG_MAP_HDR_LEN;
                _hdr_info->log_level    = log_client_log_level;
                _hdr_info->log_line_num = DEF_LOG_BUF_SIZE / DEF_LOG_LINE_LEN;
                _hdr_info->cur_line     = 0;
                _hdr_info->log_line_len = DEF_LOG_LINE_LEN;
                _hdr_info->stat_len     = DEF_STAT_LEN;
                _hdr_info->stat_gap     = DEF_STAT_GAP;

                _hdr_info->ip = 0;
                _hdr_info->port_count = 0;
            }
            catch (ipc_ex& ex) {
                _shm = CShm::open(shm_key, DEF_LOG_MAP_SIZE);
                _hdr_info = (LogMapHdr*)_shm->memory();
                if (0 > check_log_header(_hdr_info, shm_key))
                {
                    fprintf(stderr, "open shm:%d for logclient failed, err:%s\n", shm_key, ex.what());
                    return -1;
                }
            }
        }
        catch (ipc_ex& ex) {
            fprintf(stderr, "create shm:%d for logclient failed, err:%s\n", shm_key, ex.what());
            return -1;
        }

        _log_hdr    = *_hdr_info;
        _log_blk    = _shm->memory() + LOG_MAP_HDR_LEN;
        _stat_blk   = _log_blk + (_log_hdr.log_line_len * _log_hdr.log_line_num);
        _remote_info_blk = _stat_blk + DEF_STAT_LEN;
        _read_line  = (_log_hdr.cur_line - 10) % _log_hdr.log_line_num;
        _shm_key = shm_key;

        _inited = true;
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
 * change_log_level():  change log level
 * @level:              log level, start from 0 (log everything)
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::change_log_level(unsigned char level)
{
    if (!_inited)
    {
        return -1;
    }

    _hdr_info->log_level = level;
    return 0;
}

/*
 * change_stat_gap():   change stat period
 * @stat_gap:           stat period in seconds, when set to zero, it will set to default value
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::change_stat_gap(unsigned short stat_gap)
{
    if (!_inited)
    {
        return -1;
    }

    if (stat_gap)
        _hdr_info->stat_gap = stat_gap;
    else
        _hdr_info->stat_gap = DEF_STAT_GAP;

    return 0;
}

/*
 * log_seek():          change read cursor (similar to fseek)
 * @offset:             the new position, measured in lines, is obtained by adding offset bytes to the position specified by whence
 * @whence:             if set to SEEK_SET, SEEK_CUR, or SEEK_END, the offset is relative to the start of the file, current position, or EOF, respectively
 * Returns:             none.
 */
void CLogClient::log_seek(int offset, int whence)
{
    if (!_inited)
    {
        return;
    }

    if (whence == SEEK_SET)
    {
        _read_line  = (_hdr_info->cur_line + 1 + offset) % _log_hdr.log_line_num;
    }
    else if (whence == SEEK_END)
    {
        _read_line  = (_hdr_info->cur_line + offset) % _log_hdr.log_line_num;
    }
    else
    {
        _read_line  = (_read_line + offset) % _log_hdr.log_line_num;
    }
}

/*
 * read_log():          read single line of log from shm
 * @type:               event type of this line
 * @timestamp:          the time when event happens
 * @log_info:           log_info struct/class relatived to event type
 * Returns:             0 on success, -1 on error, 1 on no more new logs.
 */
int CLogClient::read_log(unsigned& event, struct timeval& timestamp, void*& log_info)
{
    if (!_inited)
    {
        return -1;
    }

    char* read_blk = _log_blk + (_log_hdr.log_line_len * _read_line);
    timestamp = *(struct timeval*)(read_blk + sizeof(unsigned));

    unsigned cur_line = _hdr_info->cur_line;
    if (_read_line == cur_line)
    {
        char* cur_blk = _log_blk + (_log_hdr.log_line_len * cur_line);
        struct timeval cur_ts = *(struct timeval*)(cur_blk + sizeof(unsigned));

        double time_diff = ((cur_ts.tv_sec - timestamp.tv_sec )*1000.0
                + (cur_ts.tv_usec - timestamp.tv_usec)/1000.0);
        if (0 >= time_diff)
        {
            return 1;
        }
    }

    event      = *(unsigned*)read_blk;
    log_info   = read_blk + LOG_LINE_HLEN;
    _read_line = (_read_line + 1) % _log_hdr.log_line_num;

    return 0;
}

/*
 * write_log():         write a single log to shm
 * @type:               specify event type that you wish to write
 * @timestamp:          the time when event happens
 * @log_info:           log_info struct/class relatived to event type
 * Returns:             0 on success, -1 on error, 1 on ignore.
 */
int CLogClient::write_log(unsigned event, struct timeval* timestamp, void* log_info)
{
    if (!_inited)
    {
        return -1;
    }

    if (GET_LOGLEVEL(event) < _hdr_info->log_level)
    {
        return 1;
    }

    /* get new current line */
    unsigned cur_line = _log_hdr.cur_line;

    /* move to new dest address */
    char* dest_addr = _log_blk + (cur_line * _log_hdr.log_line_len);

    switch(GET_LOGTYPE(event))
    {
        case LOG4NET:
        {
            NetLogInfo* net_log = reinterpret_cast<NetLogInfo*>(log_info);
            *(unsigned*)dest_addr = event;
            *(struct timeval*)(dest_addr + sizeof(unsigned)) = *timestamp;
            NetLogInfo* dst_log = (NetLogInfo*)(dest_addr + LOG_LINE_HLEN);
            dst_log->flow        = net_log->flow;
            dst_log->remote_ip   = net_log->remote_ip;
            dst_log->local_ip    = net_log->local_ip;
            dst_log->remote_port = net_log->remote_port;
            dst_log->local_port  = net_log->local_port;
            dst_log->data_len    = net_log->data_len;
            dst_log->mq_index    = net_log->mq_index;
            dst_log->err         = net_log->err;
            dst_log->wait_time   = net_log->wait_time;
            break;
        }
        case LOG4MEM:
        {
            MemLogInfo* mem_log = reinterpret_cast<MemLogInfo*>(log_info);
            *(unsigned*)dest_addr = event;
            *(struct timeval*)(dest_addr + sizeof(unsigned)) = *timestamp;
            MemLogInfo* dst_log = (MemLogInfo*)(dest_addr + LOG_LINE_HLEN);
            dst_log->flow        = mem_log->flow;
            dst_log->remote_ip   = mem_log->remote_ip;
            dst_log->local_ip    = mem_log->local_ip;
            dst_log->remote_port = mem_log->remote_port;
            dst_log->local_port  = mem_log->local_port;
            dst_log->old_size    = mem_log->old_size;
            dst_log->new_size    = mem_log->new_size;
            break;
        }
        case LOG4MCD:
        {
            MCDLogInfo* mcd_log = reinterpret_cast<MCDLogInfo*>(log_info);
            *(unsigned*)dest_addr = event;
            *(struct timeval*)(dest_addr + sizeof(unsigned)) = *timestamp;
            MCDLogInfo* dst_log = (MCDLogInfo*)(dest_addr + LOG_LINE_HLEN);
            dst_log->flow        = mcd_log->flow;
            dst_log->ip          = mcd_log->ip;
            dst_log->port        = mcd_log->port;
            dst_log->local_port  = mcd_log->local_port;
            dst_log->wait_time   = mcd_log->wait_time;
            dst_log->mq_index    = mcd_log->mq_index;
            dst_log->err         = mcd_log->err;
            dst_log->seq         = mcd_log->seq;
            break;
        }
        case LOG4NS:
        {
            NSLogInfo* ns_log = reinterpret_cast<NSLogInfo*>(log_info);
            *(unsigned*)dest_addr = event;
            *(struct timeval*)(dest_addr + sizeof(unsigned)) = *timestamp;
            NSLogInfo* dst_log = (NSLogInfo*)(dest_addr + LOG_LINE_HLEN);
            dst_log->retcode   = ns_log->retcode;
            dst_log->event_id  = ns_log->event_id;
            break;
        }

        default:
            return -1;
    }

    cur_line = (cur_line + 1) % _log_hdr.log_line_num;

    /* update log write cursor for writer */
    _log_hdr.cur_line = cur_line;

    /* update log write cursor for reader */
    _hdr_info->cur_line = cur_line;

    return 0;
}

/*
 * read_stat():         read stats from shm
 * @type:               specify stat type (enum tagStatType) that you wish to read
 * @stat_info:          stat_info struct/class relatived to stat type
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::read_stat(unsigned type, void*& stat_info)
{
    if (!_inited)
    {
        return -1;
    }

    switch(type)
    {
        case STAT_NET:
        {
            stat_info = _stat_blk;
            break;
        }
        case STAT_MCD:
        {
            stat_info = (_stat_blk + NET_STAT_BLK_SIZE);
            break;
        }

        default:
            return -1;
    }

    return 0;
}

/*
 * write_stat():        write stats to shm
 * @type:               stat type (enum tagStatType)
 * @stat_info:          stat_info struct/class relatived to stat type
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::write_stat(unsigned type, void* stat_info)
{
    if (!_inited)
    {
        return -1;
    }

    switch(type)
    {
        case STAT_NET:
        {
            memcpy(_stat_blk, stat_info, sizeof(CCDStatInfo));
            break;
        }
        case STAT_MCD:
        {
            memcpy(_stat_blk + NET_STAT_BLK_SIZE, stat_info, sizeof(MCDStatInfo));
            break;
        }

        default:
            return -1;
    }

    return 0;
}

/*
 * read_remote_info():  read remote info from shm
 * @remote_info:        remote info struct/class relatived to type
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::read_remote_info(void*& remote_info, uint32_t *data_len, uint32_t *item_num) {
    if (!_inited) {
        return -1;
    }
    *data_len = *reinterpret_cast<uint32_t*>(_remote_info_blk);
    *item_num = *reinterpret_cast<uint32_t*>(_remote_info_blk + sizeof(uint32_t));
    remote_info = _remote_info_blk + sizeof(*data_len) + sizeof(*item_num);
    return 0;
}

/*
 * write_stat():        write stats to shm
 * @remote_info:        remote_info struct/class relatived to type
 * Returns:             0 on success, -1 on error.
 */
int CLogClient::write_remote_info(void* remote_info, uint32_t data_len, uint32_t item_num) {
    if (!_inited) {
        return -1;
    }
    *reinterpret_cast<uint32_t*>(_remote_info_blk) = data_len;
    *reinterpret_cast<uint32_t*>(_remote_info_blk + sizeof(uint32_t)) = item_num;
    memcpy(_remote_info_blk + sizeof(data_len) + sizeof(item_num), remote_info, data_len);
    return 0;
}

/*
 * is_inited():         Whether log map has been initialized.
 * Returns:             true when client has been initialized, false when client has not been initialized.
 */
bool CLogClient::is_inited()
{
    return _inited;
}

void CLogClient::DebugShmKey() {
    printf("log_client.shm_key: %x\n", _shm_key);
}

bool CLogClient::SetIpWithInnerIp() {
    if (!_inited) {
        return false;
    }
    bool ret = true;
    uint32_t ip;
    ret = CLogClient::GetLocalIp(&ip);
    _hdr_info->ip = ip;
    return ret;
}

void CLogClient::GetIp(uint32_t *ip) {
    *ip = _hdr_info->ip;
}

bool CLogClient::SetCCDPort(const tfc::base::CFileConfig& page) {
    if (!_inited) {
        return false;
    }
    const static uint32_t kReportPortSize = 10;
    uint32_t port = 0;
    _hdr_info->port_count = 0;
    for (uint32_t i = 0; i < kReportPortSize; i++) {
        try {
            std::stringstream ss;
            if (i == 0) {
                ss << "root\\bind_port";
            } else {
                ss << "root\\bind_port" << (i + 1);
            }
            port = (uint32_t)strtol(page[ss.str()].c_str(), (char**)NULL, 10);
            _hdr_info->port[_hdr_info->port_count++] = port;
        } catch(...) {
        }
    }
    uint32_t remain_ports_num = kReportPortSize - _hdr_info->port_count;
    for (uint32_t i = 0; i < remain_ports_num; i++) {
        try {
            std::stringstream ss;
            if (i == 0) {
                ss << "root\\udp\\bind_port";
            } else {
                ss << "root\\udp\\bind_port" << (i + 1);
            }
            port = (uint32_t)strtol(page[ss.str()].c_str(), (char**)NULL, 10);
            _hdr_info->port[_hdr_info->port_count++] = port;
        } catch(...) {
        }
    }
    return true;
}

void CLogClient::GetCCDPort(uint32_t *port_buf, uint32_t port_buf_len, uint32_t *port_num, uint32_t *mini_port_out) {
    *port_num = _hdr_info->port_count;
    uint32_t mini_port = 65536 + 1;
    for (uint32_t i = 0; i < _hdr_info->port_count && i < port_buf_len; i++) {
        port_buf[i] = _hdr_info->port[i];
        if (port_buf[i] < mini_port) {
            mini_port = port_buf[i];
        }
    }
    if (mini_port != 65536 + 1) {
        *mini_port_out = mini_port;
    }
}

bool CLogClient::SetSoMd5(const char *buf, uint32_t len) {
    if (!_inited) {
        return false;
    }
    uint32_t min = (len <= sizeof(_hdr_info->so_md5) ? len : sizeof(_hdr_info->so_md5));
    memcpy(_hdr_info->so_md5, buf, min);
    return true;
}

void CLogClient::GetMd5(std::string *md5) {
    // md5->assign(_hdr_info->so_md5, strlen(_hdr_info->so_md5));
    md5->assign(_hdr_info->so_md5, 32);
}

bool CLogClient::SetMcpppVersion(const char *buf, uint32_t len) {
    if (!_inited) {
        return false;
    }
    uint32_t min = (len <= sizeof(_hdr_info->mcppp_version) ? len : sizeof(_hdr_info->mcppp_version));
    memcpy(_hdr_info->mcppp_version, buf, min);
    return true;
}

void CLogClient::GetMcpppVersion(char *buf, uint32_t len) {
    uint32_t min = (len <= 32 ? len : 32);
    memcpy(buf, _hdr_info->mcppp_version, min);
}

bool CLogClient::SetMcpppCompilingDate(const char *buf, uint32_t len) {
    if (!_inited) {
        return false;
    }
    uint32_t min = (len <= sizeof(_hdr_info->mcppp_compiling_date) ? len : sizeof(_hdr_info->mcppp_compiling_date));
    memcpy(_hdr_info->mcppp_compiling_date, buf, min);
    return true;
}

void CLogClient::GetMcpppCompilingDate(char *buf, uint32_t len) {
    uint32_t min = (len <= 32 ? len : 32);
    memcpy(buf, _hdr_info->mcppp_compiling_date, min);
}

bool CLogClient::GetLocalIp(uint32_t *ip) {
    struct in_addr addr;
    inet_aton("10.0.0.0", &addr);
    uint32_t ip10 = addr.s_addr;
    inet_aton("255.0.0.0", &addr); // 10.0.0.0/8
    uint32_t ip10_mask = addr.s_addr;

    inet_aton("172.16.0.0", &addr);
    uint32_t ip172 = addr.s_addr;
    inet_aton("255.240.0.0", &addr); // 172.16.0.0/12
    uint32_t ip172_mask = addr.s_addr;

    inet_aton("192.168.0.0", &addr);
    uint32_t ip192 = addr.s_addr;
    inet_aton("255.255.0.0", &addr); // 192.168.0.0/16
    uint32_t ip192_mask = addr.s_addr;

    *ip = 0;
    struct ifaddrs *ptr = NULL;
    if (getifaddrs(&ptr) != 0) {
        fprintf(stderr, "getifaddrs() error: %m.\n");
        return false;
    }
    for (struct ifaddrs *i = ptr; i != NULL; i = i->ifa_next) {
        if (i->ifa_addr->sa_family == AF_INET) { // ipv4
            uint32_t addr = *reinterpret_cast<uint32_t*>(&((struct sockaddr_in*)i->ifa_addr)->sin_addr);
            if ((addr & ip10_mask) == ip10 ||
                (addr & ip172_mask) == ip172 ||
                (addr & ip192_mask) == ip192) {
                *ip = addr;
                break;
            }
        } else if (i->ifa_addr->sa_family == AF_INET6) { // ipv6
            // void *addr_ptr = &((struct sockaddr_in6*)i->ifa_addr)->sin6_addr;
        }
    }
    if (ptr != NULL) {
        freeifaddrs(ptr);
    }
    return (*ip != 0);
}
