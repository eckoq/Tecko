#ifndef _MYDAEMON_H_
#define _MYDAEMON_H_

extern bool stop;				//true-ÔËÐÐ£¬false-ÍË³ö
extern void mydaemon(const char* name);	
extern void initenv(const char* name);
extern void cpubind(const char* name, int cpuid);
#endif
