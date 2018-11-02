// Copyright 2013, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
//
#include "mcppp_info.h"

#include <string.h>

#include "stat.h"
#include "watchdog_log.h"
#include "watchdog_common.h"

namespace watchdog {
namespace mcppp {

McpppInfo::McpppInfo() {
}

McpppInfo::~McpppInfo() {
	std::map<std::string, struct InfoItem>::iterator it;
	for (it = m_info_items.begin(); it != m_info_items.end(); ++it) {
		if (NULL != (it->second).client) {
			delete (it->second).client;
		}
	}
}

bool McpppInfo::AddInfoItem(int argc, char *argv[]) {
	for (int i = 0; i < argc; i++) {
		std::string word = argv[i];
		if (word.find("conf") != std::string::npos) {
			return AddInfoItem(word);
		}
	}
    log_sys("Cannot add info item for bin name: %s\n", argv[0]);
	return false;
}

bool McpppInfo::AddInfoItem(const std::string& name, bool is_mcd) {
	if (m_info_items.find(name) != m_info_items.end()) {
        log_sys("%s is already under control.\n", name.c_str());
		return false;
	}
	struct InfoItem info_item;
	info_item.client = new tools::CLogClient();
	if (is_mcd || name.find("mcd") != std::string::npos) {
		info_item.type = tools::STAT_MCD;
	} else { // check the conf name, otherwise use tools::Unkown
		if (name.find("ccd") != std::string::npos || name.find("dcc") != std::string::npos) {
			info_item.type = tools::STAT_NET;
		} else {
            log_sys("%s is not a mcd, ccd or dcc conf. Use STAT_UNKOWN.\n", name.c_str());
			info_item.type = tools::STAT_UNKOWN;
		}
	}
	if (info_item.client->init(name) != 0) {
		log_sys("Init log client error\n");
		return false;
	}
	m_info_items[name] = info_item;
	log_sys("Add info item for %s\n", name.c_str());
	return true;
}

bool McpppInfo::RemoveInfoItem(int argc, char *argv[]) {
	for (int i = 0; i < argc; i++) {
		std::string word = argv[i];
		if (word.find("conf") != std::string::npos) {
			return RemoveInfoItem(word);
		}
	}
	log_sys("Cannot remove info item with bin name: %s\n", argv[0]);
	return false;
}

bool McpppInfo::RemoveInfoItem(const std::string& name) {
	std::map<std::string, struct InfoItem>::iterator it = m_info_items.find(name);
	if (it == m_info_items.end()) {
		log_sys("remove a non-exist info item with conf: %s\n", name.c_str());
		return false;
	}
	delete (it->second).client;
	log_sys("Remove the info item for %s\n", it->first.c_str());
	m_info_items.erase(it);
	return true;
}

bool McpppInfo::ReadStatInfo(char *buf, uint32_t buf_len, int *data_len) {
    struct WtgReportInfo *stats = reinterpret_cast<struct WtgReportInfo*>(buf);
    if (buf_len < sizeof(WtgReportInfo)) {
        log_sys("buffer is too small for read process stat info.!\n");
        return false;
    }
    memset(buf, 0, sizeof(WtgReportInfo));
    stats->type = TYPE_VERSION_1; // WtgReportInfo type/version
    uint32_t read_size = 0;

	std::map<std::string, struct InfoItem>::iterator it;
	for (it = m_info_items.begin(); it != m_info_items.end(); ++it) {
        uint32_t buf_free_size = buf_len - sizeof(WtgReportInfo) - read_size;
		switch ((it->second).type) {
			case tools::STAT_MCD: {
				// update md5 here
                std::string md5;
				(it->second).client->GetMd5(&md5);
				memcpy(stats->so_md5, md5.c_str(), md5.size());
                // read stat info
				void *mcd_stat_info;
				if ((it->second).client->read_stat(tools::STAT_MCD, mcd_stat_info) != 0) {
					log_sys("read mcd stat for client %s error.\n", it->first.c_str());
                    continue;
				}
				uint32_t byte_size = 0;
				(reinterpret_cast<tools::MCDStatInfo*>(mcd_stat_info))->GetByteSize(&byte_size);
                if (buf_free_size < byte_size + sizeof(char) + sizeof(byte_size)) {
                    // mcd stat info will cost [byte_size + 1(type) + 4(sizeof(byte_size))] bytes
                    // do not copy the data
					log_sys("WtgReportStat buf size is not enough.\n");
                    continue;
                }
                stats->report_info[read_size] = (char)(tools::STAT_MCD); // type
                memcpy(stats->report_info + read_size + sizeof(char), (char*)(&byte_size), sizeof(byte_size)); // len
                uint32_t data_len = 0; // data
                if (!(reinterpret_cast<tools::MCDStatInfo*>(mcd_stat_info))->ToString(
                            stats->report_info + read_size + sizeof(char) + sizeof(byte_size), byte_size, &data_len)) {
                    log_sys("MCDStatInfo tostring error.\n");
                    continue;
                }
                // update read_size as buf pos.
                read_size += sizeof(char);
                read_size += sizeof(byte_size);
                read_size += byte_size;
                stats->count++;
				break;
			}
			case tools::STAT_NET: {
                // update ip, version_string, compiling_date here, ip/port is in ccd log client header
                (it->second).client->GetMcpppVersion(stats->mcppp_version, 32);
                (it->second).client->GetMcpppCompilingDate(stats->mcppp_compiling_date, 32);
                (it->second).client->GetIp(&stats->ip);
				void *net_stat_info;
				if ((it->second).client->read_stat(tools::STAT_NET, net_stat_info) != 0) {
					log_sys("read ccd/dcc stat for client %s error.\n", it->first.c_str());
                    continue;
				}
                if ((reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->is_ccd == 1) {
                    // if the stat info belongs to ccd, update port info here
                    (it->second).client->GetCCDPort(stats->port, 10, &stats->port_count, &stats->mini_port);
                }
				uint32_t byte_size = 0;
				(reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->GetByteSize(&byte_size);
                if (buf_free_size < byte_size + sizeof(char) + sizeof(byte_size)) {
                    // ccd/dcc stat info will cost [byte_size + 1(type) + 4(sizeof(byte_size))] bytes
                    // do not copy the data
					log_sys("WtgReportStat buf size is not enough for ccd/dcc stat info.\n");
                    continue;
                }
                stats->report_info[read_size] = (char)(tools::STAT_NET); // type
                memcpy(stats->report_info + read_size + sizeof(char), (char*)(&byte_size), sizeof(byte_size)); // len
                uint32_t data_len = 0; // data
                if (!(reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->ToString(
                            stats->report_info + read_size + sizeof(char) + sizeof(byte_size), byte_size, &data_len)) {
                    log_sys("CCDStatInfo tostring error.\n");
                    continue;
                }
                // update read_size as buf pos.
                read_size += sizeof(char);
                read_size += sizeof(byte_size);
                read_size += byte_size;
                stats->count++;
				break;
			}
			case tools::STAT_UNKOWN: {
				log_sys("should not be here in ReadStatInfo. Read unkown type stat.\n");
				break;
			}
			default:
				log_sys("should not be here. no type.\n");
				break;
		}
	}
    stats->report_len = read_size;
    *data_len = sizeof(WtgReportInfo) + stats->report_len;
    return true;
}

bool McpppInfo::ReadRemoteInfo(char *buf, uint32_t buf_len, int *data_len) {
    struct WtgReportInfo *info = reinterpret_cast<WtgReportInfo*>(buf);
    if (buf_len < sizeof(WtgReportInfo)) {
        log_sys("buffer is too small for read process remote info.!\n");
        return false;
    }
    memset(buf, 0, sizeof(WtgReportInfo));
    info->type = TYPE_VERSION_1; // WtgReportInfo type/version
    uint32_t read_size = 0;

	std::map<std::string, struct InfoItem>::iterator it;
	for (it = m_info_items.begin(); it != m_info_items.end(); ++it) {
        uint32_t buf_free_size = buf_len - sizeof(WtgReportInfo) - read_size;
		switch ((it->second).type) {
			case tools::STAT_MCD: {
                // up md5
                std::string md5;
				(it->second).client->GetMd5(&md5);
				memcpy(info->so_md5, md5.c_str(), md5.size());
                break;
            }
			case tools::STAT_NET: {
                // up ip, version, compiling date
                (it->second).client->GetMcpppVersion(info->mcppp_version, 32);
                (it->second).client->GetMcpppCompilingDate(info->mcppp_compiling_date, 32);
                (it->second).client->GetIp(&info->ip);
				void *net_stat_info;
				if ((it->second).client->read_stat(tools::STAT_NET, net_stat_info) != 0) {
					log_sys("read ccd/dcc stat for client %s error.\n", it->first.c_str());
                    continue;
				}
                if ((reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->is_ccd == 1) {
                    // skip ccd, only dcc has remote info; update port info here
                    (it->second).client->GetCCDPort(info->port, 10, &info->port_count, &info->mini_port);
                    continue;
                }
                void *remote_info;
                uint32_t remote_info_len = 0;
                uint32_t item_num = 0;
				if ((it->second).client->read_remote_info(remote_info, &remote_info_len, &item_num) != 0) {
					log_sys("read dcc remote info for client %s error.\n", it->first.c_str());
                    continue;
				}
                if (item_num != remote_info_len / sizeof(tools::RemoteInfo)) {
                    log_sys("bad remote info content.\n");
                    continue;
                }
                if (buf_free_size < remote_info_len) {
                    log_sys("buffer is not enough for store remote info.\n");
                    continue;
                }
                memcpy(info->report_info + read_size, reinterpret_cast<char*>(remote_info), remote_info_len);
                read_size += remote_info_len;
                info->count += item_num;
                break;
            }
			case tools::STAT_UNKOWN: {
				log_sys("should not be here in ReadRemoteInfo. Read unkown type stat.\n");
				break;
			}
			default:
				log_sys("should not be here. no type.\n");
        }
    }
    info->report_len = read_size;
    *data_len = sizeof(WtgReportInfo) + info->report_len;
    return true;
}

bool McpppInfo::ReadLogInfo(char *buf, uint32_t buf_len, int *data_len, int log_type) {
    struct WtgReportInfo *info = reinterpret_cast<WtgReportInfo*>(buf);
    if (buf_len < sizeof(WtgReportInfo)) {
        log_sys("buffer is too small for read log info.!\n");
        return false;
    }
    memset(buf, 0, sizeof(WtgReportInfo));
    info->type = TYPE_VERSION_1; // WtgReportInfo type/version
    uint32_t read_size = 0;

	std::map<std::string, struct InfoItem>::iterator it;
	for (it = m_info_items.begin(); it != m_info_items.end(); ++it) {
		switch ((it->second).type) {
			case tools::STAT_MCD: {
                // up md5
                std::string md5;
				(it->second).client->GetMd5(&md5);
				memcpy(info->so_md5, md5.c_str(), md5.size());

                if (log_type != MCD_LOG) {
                    continue;
                }
                unsigned event;
                struct timeval timestamp;
                void* log_info;
                uint32_t read_log_line_num = 0;
                while (read_log_line_num <= 200) {
                    read_log_line_num++;
                    int ret = (it->second).client->read_log(event, timestamp, log_info);
                    if(ret != 0) {
                        // no log read
                        break;
                    }
                    uint32_t buf_free_size = buf_len - sizeof(WtgReportInfo) - read_size;
                    uint32_t data_len;
                    if (!LogInfoToString(info->report_info + read_size, buf_free_size, &data_len, event, timestamp, log_info)) {
                        log_sys("buffer is too small for store mcd log info.!\n");
                        break;
                    }
                    read_size += data_len;
                    info->count++;
                }
                break;
            }
			case tools::STAT_NET: {
                // update ip, port, version, compiling date
                (it->second).client->GetMcpppVersion(info->mcppp_version, 32);
                (it->second).client->GetMcpppCompilingDate(info->mcppp_compiling_date, 32);
                (it->second).client->GetIp(&info->ip);
				void *net_stat_info;
				if ((it->second).client->read_stat(tools::STAT_NET, net_stat_info) != 0) {
					log_sys("read ccd/dcc stat for client %s error.\n", it->first.c_str());
                    continue;
				}
                if ((reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->is_ccd == 1) {
                    (it->second).client->GetCCDPort(info->port, 10, &info->port_count, &info->mini_port);
                    if (log_type != CCD_LOG) {
                        // skip ccd while you want dcc_log
                        continue;
                    }
                }
                if ((reinterpret_cast<tools::CCDStatInfo*>(net_stat_info))->is_ccd != 1 && log_type != DCC_LOG) {
                    // skip dcc while you want ccd_log
                    continue;
                }

                unsigned event;
                struct timeval timestamp;
                void* log_info;
                uint32_t read_log_line_num = 0;
                // (it->second).client->log_seek(-1, SEEK_END);
                while (read_log_line_num <= 200) {
                    read_log_line_num++;
                    int ret = (it->second).client->read_log(event, timestamp, log_info);
                    if(ret != 0) {
                        // no log read
                        log_sys("no more log to read!\n");
                        break;
                    }
                    uint32_t buf_free_size = buf_len - sizeof(WtgReportInfo) - read_size;
                    uint32_t data_len;
                    if (!LogInfoToString(info->report_info + read_size, buf_free_size, &data_len, event, timestamp, log_info)) {
                        log_sys("buffer is too small for store ccd log info.!\n");
                        break;
                    }
                    read_size += data_len;
                    info->count++;
                }
                break;
            }
			case tools::STAT_UNKOWN: {
				log_sys("should not be here in ReadLogInfo. Read unkown type log info.\n");
				break;
			}
			default:
				log_sys("should not be here. no type.\n");
        }
    }
    info->report_len = read_size;
    *data_len = sizeof(WtgReportInfo) + info->report_len;
    return true;
}
bool McpppInfo::LogInfoToString(char *buf, uint32_t buf_len, uint32_t *data_len, uint32_t event, struct timeval tv, void* log_info) {
    // ToString format: | len | event | timeval | info |
    switch(GET_LOGTYPE(event)) {
        case tools::LOG4NET: {
            tools::NetLogInfo* info = (tools::NetLogInfo*)log_info;
            *data_len = sizeof(uint32_t) + sizeof(event) + sizeof(*info) + sizeof(struct timeval);
            if (buf_len < *data_len) {
                *data_len = 0;
                return false;
            }
            *reinterpret_cast<uint32_t*>(buf) = *data_len;
            *reinterpret_cast<uint32_t*>(buf + sizeof(*data_len)) = event;
            *reinterpret_cast<struct timeval*>(buf + sizeof(*data_len) + sizeof(event)) = tv;
            memcpy(buf + sizeof(*data_len) + sizeof(event) + sizeof(tv), reinterpret_cast<char*>(info), sizeof(*info));
            break;
        }
        case tools::LOG4MEM: {
            tools::MemLogInfo* info = (tools::MemLogInfo*)log_info;
            *data_len = sizeof(uint32_t) + sizeof(event) + sizeof(*info) + sizeof(struct timeval);
            if (buf_len < *data_len) {
                *data_len = 0;
                return false;
            }
            *reinterpret_cast<uint32_t*>(buf) = *data_len;
            *reinterpret_cast<uint32_t*>(buf + sizeof(*data_len)) = event;
            *reinterpret_cast<struct timeval*>(buf + sizeof(*data_len) + sizeof(event)) = tv;
            memcpy(buf + sizeof(*data_len) + sizeof(event) + sizeof(tv), reinterpret_cast<char*>(info), sizeof(*info));
            break;
        }
        case tools::LOG4MCD: {
            tools::MCDLogInfo* info = (tools::MCDLogInfo*)log_info;
            *data_len = sizeof(uint32_t) + sizeof(event) + sizeof(*info) + sizeof(struct timeval);
            if (buf_len < *data_len) {
                *data_len = 0;
                return false;
            }
            *reinterpret_cast<uint32_t*>(buf) = *data_len;
            *reinterpret_cast<uint32_t*>(buf + sizeof(*data_len)) = event;
            *reinterpret_cast<struct timeval*>(buf + sizeof(*data_len) + sizeof(event)) = tv;
            memcpy(buf + sizeof(*data_len) + sizeof(event) + sizeof(tv), reinterpret_cast<char*>(info), sizeof(*info));
            break;
        }
        case tools::LOG4NS: {
            tools::NSLogInfo* info = (tools::NSLogInfo*)log_info;
            *data_len = sizeof(uint32_t) + sizeof(event) + sizeof(*info) + sizeof(struct timeval);
            if (buf_len < *data_len) {
                *data_len = 0;
                return false;
            }
            *reinterpret_cast<uint32_t*>(buf) = *data_len;
            *reinterpret_cast<uint32_t*>(buf + sizeof(*data_len)) = event;
            *reinterpret_cast<struct timeval*>(buf + sizeof(*data_len) + sizeof(event)) = tv;
            memcpy(buf + sizeof(*data_len) + sizeof(event) + sizeof(tv), reinterpret_cast<char*>(info), sizeof(*info));
            break;
        }
        default: {
            // log_sys("error event\n");
            *data_len = sizeof(uint32_t) + sizeof(event) + sizeof(struct timeval);
            if (buf_len < *data_len) {
                *data_len = 0;
                return false;
            }
            *reinterpret_cast<uint32_t*>(buf) = *data_len;
            *reinterpret_cast<uint32_t*>(buf + sizeof(*data_len)) = event;
            *reinterpret_cast<struct timeval*>(buf + sizeof(*data_len) + sizeof(event)) = tv;
            break;
        }
    }
    return true;
}

} // namespace mcppp
} // namespace watchdog
