
#ifndef _TFC_NET_OPEN_MQ_H_
#define _TFC_NET_OPEN_MQ_H_

#include "tfc_net_ipc_mq.h"
#include <string>

//////////////////////////////////////////////////////////////////////////

namespace tfc {	namespace net {
	CFifoSyncMQ* GetMQ(const std::string& conf_file);
//	CFifoSyncMQ* GetMQ(const std::string& conf_file, bool to_read);
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_OPEN_MQ_H_
///:~
