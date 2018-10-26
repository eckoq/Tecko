/*
 * common_proto.h:		IPC protocol for MCP++ or NWS wtg module.
 * Date:					2011-04-08
 */

#ifndef __COMMON_PROTO_H
#define __COMMON_PROTO_H

#include <sys/cdefs.h>

#pragma pack(1)

__BEGIN_DECLS

#ifdef __FOR_IDE								// Never define.
struct __for_ide {};
#endif

#define PORTS_LEN			200					// PORTs string length.

#ifndef VER_STR_LEN
#define VER_STR_LEN			64					// Version string length.
#endif

#ifndef BASE_ITEM_SIZE
#define BASE_ITEM_SIZE		256					// Base information string item max length.
#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX		108					// UNIX domain address path length. linux/un.h
#endif

#define WTG_IPC_MAGIC		0xab99ef26			// IPC head magic number.

/*
 * Report type.
 */
enum {
	IPC_TYPE_BASE = 1,							// Base information.
	IPC_TYPE_ATTR = 2,							// Attribute status.
	IPC_TYPE_ALARM = 3,							// Alarm message.
	IPC_TYPE_LOG = 4,							// Log message.
	IPC_TYPE_PROC = 5							// Process information.
};

/*
 * Common header.
 */
typedef struct {
	unsigned int	magic;						// Magic number.
	char			type;						// Data type.
	int				len;						// Data length.
	char			data[0];					// Data.
} wtg_ipc_head_t;

/*
 * Base information packet.
 */
typedef struct {
	char			ver[VER_STR_LEN];			// MCP++ module or NWS version.
	char			ports[PORTS_LEN];			// PORTs. Divid by ','. Must terminated by '\0.'
	char			plugins[BASE_ITEM_SIZE];	// Plugins. Divid by ';'. Must terminated by '\0.'
	char			prog_path[BASE_ITEM_SIZE];	// Binary program path. Must terminated by '\0.'
	char			conf_path[BASE_ITEM_SIZE];	// Configure file path. Must terminated by '\0.'
} wtg_base_t;

/*
 * Attribute status information.
 */
typedef struct {
	unsigned short	length;						// Packet length.
	char			data[0];					// Content.
} wtg_attr_t;

__END_DECLS

#pragma pack()

#endif

