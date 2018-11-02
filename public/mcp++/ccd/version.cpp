#include "version.h"
#include <string.h>

char version_string[] = "mcp++ : 2.3.7";
char compiling_date[] = "build date: "__DATE__;

int get_version_num() {
	char* p = strstr(version_string, ":");
	if(p) {
		do {++p;}while(*p == ' ');
		char* q = p;
		unsigned v = 0;
		while(*q) {
			if(*q == '.') {
				v = v * 10 + *p - '0';
				p = q + 1;
			}
			++q;
		}
		v = v * 10 + *p - '0';
		return v;
	}
	return 0;
}
