// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "base/tfc_md5.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <list>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

inline void md5sum(const char* buf, size_t len, char new_buf[16]) {
    MD5_CTX md5;

    MD5Init(&md5);
    MD5Update(&md5, (u_char*)(buf), len);
    MD5Final((u_char*)(new_buf), &md5);
}

int list_mcp_config(const std::string& dir_name, std::list<std::string>* config_files) {
    DIR *dir;
    struct dirent *ent;
    int max_read_dir_count = 65536;

    dir = opendir(dir_name.c_str());
    if (dir == NULL) {
        std::cout << "can't find dir:" << dir_name << std::endl;
        return -1;
    }

    while ((--max_read_dir_count >= 0) && ((ent = readdir(dir)) != NULL)) {
        if (strstr(ent->d_name, ".conf") == NULL) {
            continue;
        }

        if (strstr(ent->d_name, "ccd") == NULL &&
            strstr(ent->d_name, "mcd") == NULL &&
            strstr(ent->d_name, "dcc") == NULL) {
            continue;
        }

        char max_path[256] = {0};
        std::string real_path = dir_name + "/" + ent->d_name;
        realpath(real_path.c_str(), max_path);
        config_files->push_back(max_path);
    }
    closedir(dir);
    return 0;
}

int remove_possible_log_client_shm(const std::string& config_name) {
    char md5sum_buf[16] = {0};
    md5sum(config_name.c_str(), config_name.size(), md5sum_buf);

    int key = *reinterpret_cast<int*>(md5sum_buf);
    if (key == -1) {
        key = *reinterpret_cast<int*>(md5sum_buf + 4);
    }
    if (key == -1) {
        return 0;
    }

    if (key < 0) {
        key = 0 -key;
    }
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << key;

    std::stringstream cmd;
    cmd << "ipcrm -M " << ss.str();
    std::cout << "config:" << config_name << " going to remove key:" << ss.str() << std::endl;
    system(cmd.str().c_str());
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "usage: " << argv[0] << " dir_name" << std::endl;
        return 0;
    }

    std::list<std::string> mcp_config_list;
    if (list_mcp_config(argv[1], &mcp_config_list) != 0) {
        return 0;
    }
    std::list<std::string>::iterator iter;
    for (iter = mcp_config_list.begin(); iter != mcp_config_list.end(); ++iter) {
        remove_possible_log_client_shm(*iter);
    }
    return 0;
}

