#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <string>

#ifdef _SHMMEM_ALLOC_
#include "ccd/tfc_net_open_shmalloc.h"
#include "ccd/myalloc.h"
#endif

#include "watchdog/tfc_base_watchdog_client.h"
#include "mcd/app_engine.h"
#include "base/tfc_base_config_file.h"
#include "base/tfc_base_str.h"
#include "base/tfc_base_so.h"
#include "old/tfc_ipc_sv.hpp"
#include "ccd/version.h"
#include "ccd/mydaemon.h"
#include "ccd/tfc_net_open_mq.h"

using namespace std;
using namespace tfc;
using namespace tfc::ipc;
using namespace tfc::net;
using namespace tfc::base;
using namespace tfc::cache;
using namespace tfc::watchdog;

extern CWatchdogClient* wdc;

static void clean_up_resource(int exit_code, void *ptr)
{
    if (NULL != ptr) {
        CacheProc* proc = (CacheProc*)ptr;
        delete proc;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("%s conf_file [non-daemon]\n", argv[0]);
        err_exit();
    }

    if (!strncasecmp(argv[1], "-v", 2)) {
        printf("mcd\n");
        printf("%s\n", version_string);
        printf("%s\n", compiling_date);
        err_exit();
    }

    if (!one_instance(argv[1])) {
        err_exit();
    }

    if (argc == 2) {
        mydaemon(argv[0]);
    } else {
        initenv(argv[0]);
    }

    CFileConfig& page = * new CFileConfig();

    try {
        page.Init(argv[1]);
    } catch (...) {
        fprintf(stderr, "open config fail, %s, %m\n", argv[1]);
        err_exit();
    }

    /* Bind CPU. */
    try {
        int bind_cpu = from_str<int>(page["root\\bind_cpu"]);
        cpubind(argv[0], bind_cpu);
    } catch (...) {
        /* nothing */
    }

    /*
     * open dynamic lib
     */
    bool dl_global = false;
    try {
        dl_global = from_str<bool>(page["root\\app_so_link_global"]);
    } catch (...) {
        /* nothing */
    }

    int dl_flag = RTLD_LAZY;
    if (dl_global) {
        dl_flag = RTLD_LAZY | RTLD_GLOBAL;
    }

    bool start_as_new_interface = false;
    try {
        string create_app_func = page["root\\create_app_func"];
        start_as_new_interface = false;
        fprintf(stderr, "root\\create_app_func is in %s, "
                "start as old mcp++\n", argv[1]);
    } catch (...) {
        fprintf(stderr, "root\\create_app_func is not in %s, "
                "start as CTAppEngine\n", argv[1]);
        start_as_new_interface = true;
    }

    CacheProc* proc = NULL;
    CSOFile so_file;
    int ret;

    if (start_as_new_interface == false) {
        std::string app_so_file;
        try {
            app_so_file = page["root\\app_so_file"];
        } catch (...) {
            fprintf(stderr, "no app_so_file config\n");
            err_exit();
        }

        ret = so_file.open(app_so_file, dl_flag);
        if (ret) {
            fprintf(stderr, "so_file open fail, %s, %m\n", app_so_file.c_str());
            err_exit();
        }

        typedef CacheProc* (*app_constructor)();
        std::string create_app_func;
        try {
            create_app_func = page["root\\create_app_func"];
        } catch (...) {
            fprintf(stderr, "create app_func failed\n");
            err_exit();
        }

        app_constructor constructor =
            (app_constructor)so_file.get_func(create_app_func);
        if (constructor == NULL) {
            fprintf(stderr, "so_file open func fail, %s, %m\n",
                    app_so_file.c_str());
            err_exit();
        }
        proc = constructor();
    }

    if (start_as_new_interface == true) {
        try {
            proc = new CTAppEngine();
        } catch (...) {
            fprintf(stderr, "init tae failed, start tae "
                    "root\\user_conf_file must in %s\n", argv[1]);
            err_exit();
        }
    }

    if (on_exit(clean_up_resource, proc)) {
        fprintf(stderr, "cannot register on_exit function.\n");
    }

    /* watchdog client */
    try {
        string wdc_conf = page["root\\watchdog_conf_file"];
        try {
            wdc = new CWatchdogClient;
        } catch (...) {
            fprintf(stderr, "Out of memory for watchdog client!\n");
            err_exit();
        }

        /* Get frame version. */
        char *frame_version =
            (strlen(version_string) > 0 ? version_string : NULL);

        /* Get plugin version. */
        const char *plugin_version = NULL;
        get_plugin_version pv_func =
            (get_plugin_version)so_file.get_func("get_plugin_version_func");
        if (pv_func) {
            plugin_version = pv_func();
        } else {
            plugin_version = NULL;
        }

        /* Get addition 0. */
        const char *add_0 = NULL;
        get_addinfo_0 add0_func =
            (get_addinfo_0)so_file.get_func("get_addinfo_0_func");
        if (add0_func) {
            add_0 = add0_func();
        } else {
            add_0 = NULL;
        }

        /* Get addition 1. */
        const char *add_1 = NULL;
        get_addinfo_1 add1_func =
            (get_addinfo_1)so_file.get_func("get_addinfo_1_func");
        if (add1_func) {
            add_1 = add1_func();
        } else {
            add_1 = NULL;
        }

        if (wdc->Init(wdc_conf.c_str(), PROC_TYPE_MCD, frame_version,
                      plugin_version, NULL, add_0, add_1))
        {
            fprintf(stderr, "watchdog client init fail, %s,%m\n",
                    wdc_conf.c_str());
            err_exit();
        }
    } catch(...) {
        /* watchdog 功能并不是必须的 */
        fprintf(stderr, "Watchdog not enabled!\n");
    }

    /* open mq */
    const map<string, string>& mqs = page.GetPairs("root\\mq");
    unsigned idx = 0;
    for (map<string, string>::const_iterator it = mqs.begin();
         it != mqs.end(); it++)
    {
        CFifoSyncMQ* mq = GetMQ(it->second);
        assert(mq);
        proc->_mqs[it->first] = mq;
        proc->_mq_list[it->first]._mq  = mq;
        proc->_mq_list[it->first]._idx = idx;
        ++idx;
    }

    /* open mem cache */
    const vector<string>& caches = page.GetSubPath("root\\cache");
    list< ptr<CShm> > shm_stub;
    for (vector<string>::const_iterator it = caches.begin();
        it != caches.end(); it++)
    {
        const     string cache_name = *it;
        int       shm_key =
            from_str<int>(page["root\\cache\\" + cache_name + "\\shm_key"]);
        uint64_t  shm_size =
            from_str<uint64_t>(page["root\\cache\\" + cache_name + "\\shm_size"]);
        uint64_t  node_total =
            from_str<uint64_t>(page["root\\cache\\" + cache_name + "\\node_total"]);
        uint64_t  bucket_size =
            from_str<uint64_t>(page["root\\cache\\" + cache_name + "\\bucket_size"]);
        uint64_t  chunk_total =
            from_str<uint64_t>(page["root\\cache\\" + cache_name + "\\chunk_total"]);
        uint32_t  chunk_size =
            from_str<uint32_t>(page["root\\cache\\" + cache_name + "\\chunk_size"]);
        int       use_cache_large = 0;
        try {
            int large =
                from_str<int>(page["root\\cache\\" + cache_name + "\\large"]);
            if (large == 1) {
                use_cache_large = 1;
            }
        } catch(...) {
            use_cache_large = 0;
        }

        bool bInited = true;
        ptr<CShm> shm;
        try {
            shm = CShm::create_only(shm_key, shm_size);
            memset(shm->memory(), 0, shm->size());
            bInited = true;
        } catch (ipc_ex& ex) {
            shm = CShm::open(shm_key, shm_size);
            bInited = false;

            /*
             * enforce check processes wait 100ms for shm memory initialize,
             * avoid race condition
             */
            usleep(100 * 1000);
        }
        shm_stub.push_back(shm);

        if (use_cache_large == 1) {
            tfc::cache_large::CacheAccess* ca =
                new tfc::cache_large::CacheAccess();
            ret = ca->open(shm->memory(), shm->size(), bInited, node_total,
                           bucket_size, chunk_total, chunk_size);
            if (ret) {
                fprintf(stderr, "Open large memcache fail! ret:%d\n", ret);
                err_exit();
            } else {
                fprintf(stderr, "Open large memcache: %s succ.\n",
                        cache_name.c_str());
            }

            proc->_caches_large[*it] = ca;
        } else {
            CacheAccess* ca = new CacheAccess();
            ret = ca->open(shm->memory(), shm->size(), bInited, (int)node_total,
                           (int)bucket_size, (int)chunk_total, (int)chunk_size);
            if (ret) {
                fprintf(stderr, "Open memcache fail! ret:%d\n", ret);
                err_exit();
            } else {
                fprintf(stderr, "Open memcache: %s succ.\n",
                        cache_name.c_str());
            }
            proc->_caches[*it] = ca;
        }
    }

    /* open disk cache */
    const vector<string>& disk_caches = page.GetSubPath("root\\disk_cache");
    for (vector<string>::const_iterator it = disk_caches.begin();
         it != disk_caches.end(); it++)
    {
        const     string cache_name = *it;
        int       shm_key =
            from_str<int>(page["root\\disk_cache\\" + cache_name + "\\shm_key"]);
        long      shm_size =
            from_str<long>(page["root\\disk_cache\\" + cache_name + "\\shm_size"]);
        unsigned  node_total =
            from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\node_total"]);
        unsigned  bucket_size =
            from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\bucket_size"]);

        unsigned           minchunksize;
        unsigned long long filesize;
        try {
            filesize =
                from_str<unsigned long long>(page["root\\disk_cache\\" + cache_name + "\\filesize"]);
            minchunksize =
                from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\minchunksize"]);
        } catch(...) {
            unsigned chunk_total =
                from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_total"]);
            unsigned chunk_size =
                from_str<unsigned>(page["root\\disk_cache\\" + cache_name + "\\chunk_size"]);
            filesize = (unsigned long long)chunk_total * (unsigned long long)chunk_size;
            minchunksize = chunk_size >> 1;
        }

        string cache_file =
            page["root\\disk_cache\\" + cache_name + "\\cache_file"];
        bool bInited = true;
        ptr<CShm> shm;
        try {
            shm = CShm::create_only(shm_key, shm_size);
            memset(shm->memory(), 0, shm->size());
            bInited = true;
        } catch (ipc_ex& ex) {
            shm = CShm::open(shm_key, shm_size);
            bInited = false;
        }
        shm_stub.push_back(shm);

        diskcache::CacheAccess* ca = new diskcache::CacheAccess();
        ret = ca->open(shm->memory(), shm->size(), cache_file, bInited,
                       node_total, bucket_size, filesize, minchunksize);
        if (ret) {
            fprintf(stderr, "Open diskcache fail!\n");
            err_exit();
        }

        proc->_disk_caches[*it] = ca;
    }

#ifdef _SHMMEM_ALLOC_
    /* share memory allocator */
    try {
        if (OpenShmAlloc(page["root\\shmalloc\\shmalloc_conf_file"])) {
            fprintf(stderr, "shmalloc init fail, %m\n");
            err_exit();
        } else {
            fprintf(stderr, "mcd shmalloc enable\n");
        }
    } catch(...) {
        fprintf(stderr, "mcd shmalloc disable\n");
    }
#endif

    /* 初始化为监控mq使用的epoll对象 */
    if (proc->init_epoll_4_mq()) {
        fprintf(stderr, "init_epoll_4_mq fail, %m\n");
        err_exit();
    }

    const string app_conf_file = page["root\\app_conf_file"];
    fprintf(stderr, "mcd started\n");
    proc->run(app_conf_file);

#ifdef _SHMMEM_ALLOC_
    myalloc_fini();
#endif
    fprintf(stderr, "mcd stopped\n");
    syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s mcd stopped\n", argv[0]);
    exit(0);
}
