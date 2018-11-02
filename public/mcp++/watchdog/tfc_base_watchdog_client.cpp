/*
 * tfc_base_watchdog_client.cpp:	Watchdog client class for MCP++.
 * Date:							2011-02-24
 */

#include "watchdog_api.h"
#include "tfc_base_watchdog_client.h"

using namespace tfc::watchdog;

/*
 * ~CWatchdogClient():	Destructor, release shm resource.
 */
CWatchdogClient::~CWatchdogClient()
{
	if ( inited ) {
		watchdog_clinet_destroy(&entry);
		inited = false;
	}
}

/*
 * Init():						Initialize the watchdog client. Must call this befor touch.
 * @conf_name:				Watchdog configure file path.
 * @proc_type:				Process type.
 * @frame_version:			Frame version.
 * @plugin_version:			Plugin version.
 * @server_ports:				Server PORTs. Divided by ','.
 * @add_info_0:				Addition information 0.
 * @add_info_1:				Addition information 1.
 * Returns:					0 on success, -1 on error.
 */
int
CWatchdogClient::Init(const char *conf_name,
					int proc_type, const char *frame_version, const char *plugin_version,
					const char *server_ports,
					const char *add_info_0, const char *add_info_1)
{	
	if ( !inited ) {
		if ( watchdog_client_init(&entry, conf_name,
				proc_type, frame_version, plugin_version, server_ports,
				add_info_0, add_info_1) ) {
			return -1;
		}

		inited = true;
		
		return 0;
	} else {
		return -1;
	}
}

/*
 * Touch():		Touck process tick in shm.
 * Returns:		0 on success, -1 on error.
 */
int
CWatchdogClient::Touch()
{
	return inited ? watchdog_client_touch(&entry) : -1;
}

/*
 * Index():		Get process index. Help wtg report.
 * Returns:		The process index or -1 on error.
 */
int
CWatchdogClient::Index()
{
	return inited ? watchdog_client_index(&entry) : -1;
}

/*
 * IsIinited():		Wether watchdog has been initialized.
 * Returns:		trun when client has been initialized, false when client has not been initialized.
 */
bool
CWatchdogClient::IsInited()
{
	return inited;
}

