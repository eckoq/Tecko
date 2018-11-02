// Copyright 2013, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
//
#ifndef MCPPP_INFO_H_
#define MCPPP_INFO_H_

#include <map>
#include <string>

#include "log_type.h"
#include "log_client.h"
#include "stat.h"

namespace watchdog {
namespace mcppp {

enum LogInfoType {
    CCD_LOG = 1,
    MCD_LOG,
    DCC_LOG,
};

struct InfoItem {
	tools::tagStatType type;
	tools::CLogClient *client;
};

class McpppInfo {
public:
	McpppInfo();
	~McpppInfo();
	bool AddInfoItem(int argc, char *argv[]);
	bool AddInfoItem(const std::string& name, bool is_mcd = false);
	bool RemoveInfoItem(int argc, char *argv[]);
	bool RemoveInfoItem(const std::string& name);
	bool ReadStatInfo(char *buf, uint32_t buf_len, int *data_len);
	bool ReadRemoteInfo(char *buf, uint32_t buf_len, int *data_len);
    bool ReadLogInfo(char *buf, uint32_t buf_len, int *data_len, int log_type);
private:
    bool LogInfoToString(char *buf, uint32_t buf_len, uint32_t *data_len, uint32_t event, struct timeval tv, void* log_info);
	std::map<std::string, struct InfoItem> m_info_items;
};

} // namespace mcppp
} // namespace watchdog

#endif  // MCPPP_INFO_H_
