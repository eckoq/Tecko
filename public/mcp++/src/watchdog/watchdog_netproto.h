/*
 * watchdog_netproto.h:	Watchdog private network protocol.
 * Date:					2011-03-22
 */

#ifndef __WATCHDOG_NETPROTO_H
#define __WATCHDOG_NETPROTO_H

#pragma pack(1)

__BEGIN_DECLS

#ifdef __FOR_IDE	// Never define.
struct __for_ide {};
#endif

#define PORTS_LEN			200				// PORTs string length.
#ifndef BASE_ITEM_SIZE
#define BASE_ITEM_SIZE		256				// Base information string item max length.
#endif

/*
 * CCD base information packet.
 */
typedef struct {
	unsigned long	ccd_ver;
	char			ports[PORTS_LEN];			// PORTs. Divid by ';'. Must terminated by '\0.'
	char			plugins[BASE_ITEM_SIZE];	// Plugins. Divid by ';'. Must terminated by '\0.'
	char			prog_path[BASE_ITEM_SIZE];	// Binary program path. Must terminated by '\0.'
	char			conf_path[BASE_ITEM_SIZE];	// Configure file path. Must terminated by '\0.'
} pkg_base_t;

/*
 * Status report information.
 */
typedef struct {
	unsigned short	length;			// Packet length.
	char			data[0];		// Content.
} pkg_stat_t;

__END_DECLS

#pragma pack()

#endif

