/*
 * wtg_master_plugin.cpp:			Plugin for Managed Platform Master.
 * Date:							2011-03-30
 */

#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "sproto.h"
#include "common_proto.h"

using namespace std;

// #define __PLUGIN_DEBUG
// #define __CONTENT_DEBUG

/*
 * get_attrs():		Get status attribute report.
 * @desc:			Content data.
 * @length:			Length of data.
 * @vt_attrs:			Attribute KEY VALUE report element.
 * Returns:			0 on success, -1 on error.
 */
int
get_attrs(char *desc, int length, vector<attr_p> &vt_attrs)
{
	wtg_attr_t			*stat_recv = (wtg_attr_t *)desc;
	char				*cur = NULL;
	int					left, key_len, val_len;
	string				key, val;
	unsigned long		l_val;
	attr_p				attr_elem;

	if ( length < stat_recv->length ) {
#ifdef __PLUGIN_DEBUG
		cout<<"Invalid attribute pakcet: status pakcet length - "<<stat_recv->length
			<<" desc length - "<<length<<endl;
#endif
		return -1;
	}

	left = stat_recv->length - sizeof(wtg_attr_t);
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
		val.append(cur, (size_t)val_len);
		cur += val_len;
		left -= val_len;

		l_val = (unsigned long)strtol(val.c_str(), NULL, 10);
		if ( ( l_val == 0 && errno == EINVAL )
			|| ( (l_val == (unsigned long)(LONG_MIN)
			|| l_val == (unsigned long)(LONG_MAX)) && errno == ERANGE ) ) {
#ifdef __PLUGIN_DEBUG
			cout<<"Invalid attribute value \""<<val.c_str()<<"\"."<<endl;
#endif
			return -1;
		}

		attr_elem.key = key;
		attr_elem.value = l_val;

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
 * @desc:			Content data.
 * @length:			Length of data.
 * @bi:				Base information report element.
 * Returns:			0 on success, -1 on error.
 */
int
get_base_info(char *desc, int length, base_info_req &bi)
{
	wtg_base_t			*base_recv = (wtg_base_t *)desc;

	if ( (unsigned int)length < sizeof(wtg_base_t) ) {
#ifdef __PLUGIN_DEBUG
		cout<<"Invalid base pakcet: sizeof(wtg_base_t) - "<<sizeof(wtg_base_t)
			<<" desc length - "<<length<<endl;
#endif
		return -1;
	}

	memset(&bi, 0, sizeof(base_info_req));

	strncpy(bi.frm_ver, base_recv->ver, 63);
	strncpy(bi.ports, base_recv->ports, 63);
	strncpy(bi.plugins, base_recv->plugins, 255);
	strncpy(bi.prog_path, base_recv->prog_path, 255);
	strncpy(bi.conf_path, base_recv->conf_path, 255);

#ifdef __CONTENT_DEBUG
	cout<<endl;
	cout<<"Base information pakcet:"<<endl;
	cout<<"ports: "<<base_recv->ports<<endl;
	cout<<"ver: "<<base_recv->ver<<endl;
	cout<<"plugins: "<<base_recv->plugins<<endl;
	cout<<"prog_path: "<<base_recv->prog_path<<endl;
	cout<<"conf_path"<<base_recv->conf_path<<endl;
	cout<<endl;
#endif

	return 0;
}

