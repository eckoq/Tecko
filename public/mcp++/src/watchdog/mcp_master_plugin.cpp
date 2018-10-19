/*
 * mcp_master_plugin.cpp:			Plugin for Managed Platform Master.
 * Date:							2011-03-30
 */

#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "wsproto.h"

#include "watchdog_netproto.h"

using namespace std;

#define __PLUGIN_DEBUG
#define __CONTENT_DEBUG

#ifndef TYPE_BASE
#define TYPE_BASE				1								// Report type base infromation.
#endif
#ifndef TYPE_ATTR
#define TYPE_ATTR				2								// Report type attribute.
#endif
#ifndef	BIN_CCD
#define BIN_CCD					2								// MCP++ report.
#endif

/*
 * get_attrs():		Get status attribute report.
 * @data:			Network data.
 * @length:			Length of data.
 * @attr_conf:		Text-Name ~ ID map.
 * @vt_attrs:			Attribute KEY VALUE report element.
 * Returns:			0 on success, -1 on error.
 */
int
get_attrs(char *data, int length, map<string, unsigned short> &attr_conf, vector<attr_value> &vt_attrs)
{
	pkg_stat_t			*stat_recv = (pkg_stat_t *)data;
	char				*cur = NULL;
	int					left, key_len, val_len;
	string				key, val;
	unsigned short		id;
	unsigned long		l_val;
	map<string, unsigned short>::iterator	it;
	attr_value			attr_elem;

	if ( length < stat_recv->length ) {
#ifdef __PLUGIN_DEBUG
		cout<<"Invalid attribute pakcet: status pakcet length - "<<stat_recv->length
			<<" data length - "<<length<<endl;
#endif
		return -1;
	}

	left = stat_recv->length - sizeof(pkg_stat_t);
	if ( left <= 0 ) {
#ifdef __PLUGIN_DEBUG
		cout<<"ERROR: no attribute in attribute packet!"<<endl;
#endif
		return -1;
	}

#ifdef __CONTENT_DEBUG
	cout<<endl;
	cout<<"Attribute packet:"<<endl;
	cout<<"length: "<<stat_recv->length<<endl;
#endif
	
	for ( cur = stat_recv->data; left > 0; ) {
		if ( left < (int)(sizeof(int) * 2) ) {
#ifdef __PLUGIN_DEBUG
			cout<<"Attribute packet not complete! No. 1."<<endl;
#endif
			return -1;
		}
		
		key_len = *((int *)cur);
		cur += sizeof(int);
		left -= sizeof(int);
		
		val_len = *((int *)cur);
		cur += sizeof(int);
		left -= sizeof(int);

		if ( !key_len || !val_len ) {
#ifdef __PLUGIN_DEBUG
			cout<<"Reject empty attribute KEY or VALUE!"<<endl;
#endif
			return -1;
		}

		if ( left < (key_len + val_len) ) {
#ifdef __PLUGIN_DEBUG
			cout<<"Attribute packet not complete! No. 2."<<endl;
#endif
			return -1;
		}

		key.clear();
		key.append(cur, (size_t)key_len);
		cur += key_len;
		left -= key_len;
		
		val.clear();
		key.append(cur, (size_t)val_len);
		cur += val_len;
		left -= val_len;

		it = attr_conf.find(key);
		id = it->second;

		l_val = (unsigned long)strtol(val.c_str(), NULL, 10);
		if ( ( l_val == 0 && errno == EINVAL )
			|| ( (l_val == (unsigned long)(LONG_MIN)
			|| l_val == (unsigned long)(LONG_MAX)) && errno == ERANGE ) ) {
#ifdef __PLUGIN_DEBUG
			cout<<"Invalid attribute value \""<<val.c_str()<<"\"."<<endl;
#endif
			return -1;
		}

		attr_elem.attr_id = id;
		attr_elem.attr_val = l_val;

		vt_attrs.push_back(attr_elem);

#ifdef __CONTENT_DEBUG
		cout<<"key: "<<key<<endl;
		cout<<"Value: "<<l_val<<endl;
#endif
	}

#ifdef __CONTENT_DEBUG
	cout<<endl;
#endif

	return 0;
}

/*
 * get_base_info():	Get base information report.
 * @data:			Network data.
 * @length:			Length of data.
 * @base:			Base information report element.
 * Returns:			0 on success, -1 on error.
 */
int
get_base_info(char *data, int length, base_info &base)
{
	pkg_base_t			*base_recv = (pkg_base_t *)data;

	if ( (unsigned int)length < sizeof(pkg_base_t) ) {
#ifdef __PLUGIN_DEBUG
		cout<<"Invalid base pakcet: sizeof(pkg_base_t) - "<<sizeof(pkg_base_t)
			<<" data length - "<<length<<endl;
#endif
		return -1;
	}

	base.ports = base_recv->ports;
	base.qnf_ver = base_recv->ccd_ver;
	base.plugins = base_recv->plugins;
	base.prog_path = base_recv->prog_path;
	base.conf_path = base_recv->conf_path;

#ifdef __CONTENT_DEBUG
	cout<<endl;
	cout<<"Base information pakcet:"<<endl;
	cout<<"ports: "<<base_recv->ports<<endl;
	cout<<"ccd_ver: "<<base_recv->ccd_ver<<endl;
	cout<<"plugins: "<<base_recv->plugins<<endl;
	cout<<"prog_path: "<<base_recv->prog_path<<endl;
	cout<<"conf_path"<<base_recv->conf_path<<endl;
	cout<<endl;
#endif

	return 0;
}

