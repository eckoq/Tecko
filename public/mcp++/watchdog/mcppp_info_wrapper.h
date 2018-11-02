// Copyright 2013, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
//
#include "watchdog_common.h"

typedef void CMcpppInfo;

#ifdef __cplusplus
extern "C" {
#endif

enum tagOpType {
    ITEM_IS_NULL = 1,
    ADD_ITEM_FAIL,
    REMOVE_ITEM_FAIL,
    READ_STAT_FAIL,
    READ_REMOTE_INFO_FAIL,
    READ_LOG_INFO_FAIL,
};

CMcpppInfo * new_McpppInfo();
int mcppp_info_add_info_item(CMcpppInfo *item, int argc, char *argv[]);
int mcppp_info_remove_info_item(CMcpppInfo *item, int argc, char *argv[]);
int mcppp_info_read_stat(CMcpppInfo *item, char *buf, int buf_len, int *data_len);
int mcppp_info_read_remote_info(CMcpppInfo *item, char *buf, int buf_len, int *data_len);
int mcppp_info_read_log_info(CMcpppInfo *item, char *buf, int buf_len, int *data_len, int log_type);
void delete_McpppInfo(CMcpppInfo *item);

#ifdef __cplusplus
}
#endif
