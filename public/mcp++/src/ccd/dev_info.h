/*
 * dev_info.h:			Device information report for wtg..
 * Date:				2011-05-24
 */

#ifndef __DEV_INFO_H
#define __DEV_INFO_H

/*
 * eth_flow_report():	Report eth flow.
 * @tick:				Report tick(10 sec).
 * Returns:			0 on success, -1 on error.
 */
extern int
eth_flow_report(int tick);

#endif

