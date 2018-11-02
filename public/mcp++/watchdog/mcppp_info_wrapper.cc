// Copyright 2013, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
//
#include <stdio.h>
#include "mcppp_info_wrapper.h"

#include "mcppp_info.h"

#ifdef __cplusplus
extern "C" {
#endif

CMcpppInfo * new_McpppInfo() {
	return new watchdog::mcppp::McpppInfo();
}

int mcppp_info_add_info_item(CMcpppInfo *item, int argc, char *argv[]) {
	if ((watchdog::mcppp::McpppInfo*)item == NULL) {
		return -ITEM_IS_NULL;
	}
	if (!((watchdog::mcppp::McpppInfo*)item)->AddInfoItem(argc, argv)) {
		return -ADD_ITEM_FAIL;
	}
	return 0;
}

int mcppp_info_remove_info_item(CMcpppInfo *item, int argc, char *argv[]) {
	if ((watchdog::mcppp::McpppInfo*)item == NULL) {
		return -ITEM_IS_NULL;
	}
	if (!((watchdog::mcppp::McpppInfo*)item)->RemoveInfoItem(argc, argv)) {
		return -REMOVE_ITEM_FAIL;
	}
	return 0;
}

int mcppp_info_read_stat(CMcpppInfo *item, char *buf, int buf_len, int *data_len) {
	if ((watchdog::mcppp::McpppInfo*)item == NULL) {
		return -ITEM_IS_NULL;
	}
	if (!((watchdog::mcppp::McpppInfo*)item)->ReadStatInfo(buf, buf_len, data_len)) {
		return -READ_STAT_FAIL;
	}
	return 0;
}

int mcppp_info_read_remote_info(CMcpppInfo *item, char *buf, int buf_len, int *data_len) {
    if ((watchdog::mcppp::McpppInfo*)item == NULL) {
		return -ITEM_IS_NULL;
	}
	if (!((watchdog::mcppp::McpppInfo*)item)->ReadRemoteInfo(buf, buf_len, data_len)) {
		return -READ_REMOTE_INFO_FAIL;
	}
	return 0;
}

int mcppp_info_read_log_info(CMcpppInfo *item, char *buf, int buf_len, int *data_len, int log_type) {
    if ((watchdog::mcppp::McpppInfo*)item == NULL) {
		return -ITEM_IS_NULL;
	}
	if (!((watchdog::mcppp::McpppInfo*)item)->ReadLogInfo(buf, buf_len, data_len, log_type)) {
		return -READ_LOG_INFO_FAIL;
	}
	return 0;
}

void delete_McpppInfo(CMcpppInfo *item) {
    delete (watchdog::mcppp::McpppInfo*)item;
    item = NULL;
}

#ifdef __cplusplus
}
#endif
