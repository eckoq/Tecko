/*
 * wtg_client_api.h:			Wtg client C++ APIs.
 * Date:						2011-04-12
 */

#ifndef __WTG_CLIENT_API_H
#define __WTG_CLIENT_API_H

#include "wtg_api.h"

using namespace std;

namespace tfc {
	namespace wtgapi {
		class CWtgClientApi {
		public:
			CWtgClientApi() : inited(false) {}			// Construct but not initizalie. Call init() before use.
			~CWtgClientApi();							// Destructor.

			bool IsInited() { return inited; }				// Wether object has been initialized.
			
			int Init(int type, int send_base, const char *conf_name, wtg_api_base_t *base);	// Initialize the object.
			int ReportAttr(wtg_api_attr_t *attrs, int attr_count);		// Report attribute status.
			int ReportAlarm(unsigned char level, const char *msg);	// Report alarm.
			
		private:
			bool			inited;
			wtg_entry_t		entry;
		};
	}
}

#endif

