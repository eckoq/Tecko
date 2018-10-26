/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 哈希函数类

CRC_32 具有极佳的分布特性和性能

256:
	RSHash 641000/s
	SuperFastHash 1202000/s
	CRC_32 549000/s

1024:
	RSHash 161000/s
	SuperFastHash 308000/s
	CRC_32 138000/s

***********************************************************/

#ifndef _HASHFUNC_H
#define _HASHFUNC_H

#include <sys/types.h>
#include <stdio.h>

void BuildTable32(u_int32_t aPoly);
u_int32_t CRC_32(char *data, u_int32_t len);
u_int32_t SuperFastHash(char *data, u_int32_t len);
u_int32_t RSHash(char *data, u_int32_t len);

#endif

