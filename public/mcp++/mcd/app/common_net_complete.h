// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCD_APP_COMMON_NET_COMPLETE_H
#define  MCD_APP_COMMON_NET_COMPLETE_H

#include "libhttp/tfc_base_http.h"

extern "C" {
    // Complete function for Asn1.3
    int asn13_net_complete_func(void* data, unsigned data_len);

    // Complete function for Asn2.0
    int asn20_net_complete_func(void* data, unsigned data_len);

    // Complete function for HTTP
    inline int http_net_complete_func(void* data, unsigned data_len)
    {
        int retv;
        return HTTP_CHECK_COMPLETE(data, data_len, retv);
    }
}

#endif  // MCD_APP_COMMON_NET_COMPLETE_H
