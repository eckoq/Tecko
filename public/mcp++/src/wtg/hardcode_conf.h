/*
 * hardcode_conf.h:		Hard code params.
 */

/*
 * 0 not use wtg, 1 use wtg.
 * wtg_enable = 1
 *
 * Wtg Maseer Name:
 * wtg_master_name = wtg-master.tc.qq.com
 *
 * Wtg Master server port:
 * wtg_master_port = 9801
 */

#ifndef __HARDCODE_CONF_H
#define __HARDCODE_CONF_H

#define WTG_ENABLE				1						// 0 not use wtg, 1 use wtg.
#define WTG_MASTER_NAME			"wtg-master.qq.com"		// Wtg Master name.
#define WTG_MASTER_IP			"172.24.19.58"			// Wtg Master IP. Use this when domain name could not connect.
#define WTG_MASTER_IP_EXTERN	"124.115.0.174"			// Wtg Master IP extern.
#define WTG_MASTER_PORT			9801					// Wtg Master server port.

#endif

