/*
 * tfc_base_watchdog_client.h:		Watchdog client class for MCP++.
 * Date:							2011-02-24
 */

#ifndef __TFC_BASE_WATCHDOG_CLIENT_H
#define __TFC_BASE_WATCHDOG_CLIENT_H

#include <string>
#include "watchdog_api.h"

using namespace std;

namespace tfc {
	namespace watchdog {		
		// Watchdog client.
		class CWatchdogClient {
		public:
			CWatchdogClient() : inited(false) {}		// Construct but not initizalie. Call init() before use watchdog client.
			~CWatchdogClient();					// Destructor.
			int Init(const char *conf_name,
					int proc_type, const char *frame_version, const char *plugin_version,
					const char *server_ports,
					const char *add_info_0, const char *add_info_1);	// Initialize the watchdog client.
			int Touch();								// Touch tick.
			int Index();								// Get process index. Help wtg report.
			bool IsInited();							// Wether watchdog has been initialized.
		private:
			bool				inited;					// Wether watchdog has been initialized.
			watchdog_client_t	entry;					// Watchdog client information.
		};
	}
}

#endif

