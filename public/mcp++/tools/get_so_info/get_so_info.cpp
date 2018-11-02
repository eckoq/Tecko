#include <dlfcn.h>
#include <iostream>
#include "tfc_cache_proc.h"

using namespace std;

int main(int argc, char * argv[]) {
    if (argc < 2) {
        printf("Usage: %s so_file\n", argv[0]);
        return -1;
    }

    // open the so file
    int dl_flag = RTLD_LAZY;
    void * phdl = dlopen (argv[1], dl_flag);
    if (NULL == phdl) {
        printf("so_file open fail:\n%s\n", dlerror());
        return -1;
    }

    // get the funciotn [get_so_info]
    void (*get_so_info)(tfc::cache::ReportInfoMap& info) ;
    dlerror();    /* Clear any existing error */
    char * error;
    get_so_info = (void (*)(tfc::cache::ReportInfoMap& info))dlsym(phdl, "get_so_information");
    if ((error = dlerror()) != NULL)  {
        fprintf (stderr, "%s\n", error);
        exit(1);
    }

    // call get_so_info
    tfc::cache::ReportInfoMap so_info;
    get_so_info(so_info);

    std::string business_name = so_info[tfc::cache::BUSSINESS_NAME];
    std::string rtx_contact = so_info[tfc::cache::RTX];

    std::cout << business_name << std::endl;
    std::cout << rtx_contact << std::endl;

    return 0;
}

