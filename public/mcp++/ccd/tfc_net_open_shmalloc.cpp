#include "tfc_net_open_shmalloc.h"
#include "tfc_base_config_file.h"
#include "tfc_base_str.h"
#include "myalloc.h"

#include <string.h>

using namespace tfc::net;
using namespace tfc::base;

int tfc::net::OpenShmAlloc(const string& conf_file)
{
	CFileConfig page;
	page.Init(conf_file);
	
	myalloc_alloc_conf conf;
	memset(&conf, 0x0, sizeof(myalloc_alloc_conf));
	conf.shmkey = from_str<unsigned>(page["root\\shmkey"]);
	conf.shmsize = from_str<unsigned long>(page["root\\shmsize"]);
	strcpy(conf.semname, page["root\\semname"].c_str());
	conf.minchunksize = from_str<unsigned>(page["root\\minchunksize"]);
	conf.maxchunksize = from_str<unsigned>(page["root\\maxchunksize"]);
	conf.maxchunknum = from_str<unsigned>(page["root\\maxchunknum"]);

	return myalloc_init(&conf);
}
