/*
 * wtg_client_api.cpp:			Wtg client C++ APIs.
 * Date:						2011-04-12
 */

#include "wtg_client_api.h"

using namespace std;

using namespace tfc::wtgapi;

/*
 * ~CWtgClientApi():		Destructor.
 */
CWtgClientApi::~CWtgClientApi()
{
	if ( inited ) {
		wtg_api_destroy(&entry);
		inited = false;
	}
}

/*
 * Init():					Initialize a wtg client API set.
 * @type:				Program type. WTG_API_TYPE_NWS or WTG_API_TYPE_MCP
 * @send_base:.			0 not send base information, others send base informaion. Send base information means prgram has been restart.
 * @conf_name:			Watchdog config file name, when type is WTG_API_TYPE_NWS, we ignore this param.
 * @base:				Base information. When send_base is zero, we ignore this param.
 * Returns:				0 on success, -1 on error.
 */
int
CWtgClientApi::Init(int type, int send_base, const char *conf_name, wtg_api_base_t *base)
{
	if ( !inited) {
		if ( wtg_api_init(&entry, type, send_base, conf_name, base) ) {
			return -1;
		}

		inited = true;
		
		return 0;
	} else {
		return -1;
	}
}

/*
 * ReportAttr():			Wtg client attribute status report.
 * @attrs:				Report attribute status array.
 * @attr_count:			Effective element count of attrs.
 * Returns:				0 on success, -1 on error.
 */
int
CWtgClientApi::ReportAttr(wtg_api_attr_t *attrs, int attr_count)
{
	return inited ? wtg_api_report_attr(&entry, attrs, attr_count) : -1;
}

/*
 * ReportAlarm():				Report alarm to wtg master.
 * @level:					Alarm level. 1 ~ 5, 1 is most serious.
 * @msg:					Alarm message. Terminate by '\0', and strlen(msg) should <= ALARM_MSG_SIZE - 1.
 * Returns:					0 on success, -1 on error.
 */
int
CWtgClientApi::ReportAlarm(unsigned char level, const char *msg)
{
	return inited ? wtg_api_report_alarm(&entry, level, msg) : -1;
}

