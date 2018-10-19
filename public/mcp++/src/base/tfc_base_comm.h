
#ifndef __TFC_BASE_COMM_H__
#define __TFC_BASE_COMM_H__

#ifndef _XOPEN_SOURCE 
#define _XOPEN_SOURCE
#endif

#include <string>
#include <time.h>
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cctype>

#include <stdexcept>

using namespace std;

namespace tfc { namespace base {

struct comm_error: public logic_error{ comm_error(const string& s);};

void ignore_pipe(void);
void Daemon();

/**
 * @param s �����ַ���
 * @return bool �Ƿ�ȫ��Ϊ����(0-9)
 */
inline bool is_digits(const string& s)
{
	if(s.length() == 0) return false;
	for (size_t i = 0; i < s.length(); i++){
		if (!isdigit(s.c_str()[i])) return false;
	}
	return true;
}

/**
 * @param s �����ַ���
 * @return bool �Ƿ�ȫ��Ϊ16��������(0-9/a-f/A-f)
 */
inline bool is_xdigits(const string& s)
{
	if(s.length() == 0) return false;
	for (size_t i = 0; i < s.length(); i++){
		if (!isxdigit(s.c_str()[i])) return false;
	}
	return true;
}

/**
 * @param s �����ַ���
 * @param filt filt��
 * @return string trim���ֵ
 */
inline string trim_left(const string &s,const string& filt=" ")
{
	char *head=const_cast<char *>(s.c_str());
	char *p=head;
	while(*p) {
		bool b = false;
		for(size_t i=0;i<filt.length();i++) {
			if((unsigned char)*p == (unsigned char)filt.c_str()[i]) {b=true;break;}
		}
		if(!b) break;
		p++;
	}
	return string(p,0,s.length()-(p-head));
}

/**
 * @param s �����ַ���
 * @param filt filt��
 * @return string trim���ֵ
 */
inline string trim_right(const string &s,const string& filt=" ")
{
	if(s.length() == 0) return string();
	char *head=const_cast<char *>(s.c_str());
	char *p=head+s.length()-1;
	while(p>=head) {
		bool b = false;
		for(size_t i=0;i<filt.length();i++) {
			if((unsigned char)*p == (unsigned char)filt.c_str()[i]) {b=true;break;}
		}
		if(!b) {break;}
		p--;
	}
	return string(head,0,p+1-head);
}

/**
 * trim_left+trim_right
 * @param s �����ַ���
 * @param filt filt��
 * @return string trim���ֵ
 */
inline string trim(const string &s,const string& filt=" ")
{
	return trim_right(trim_left(s, filt),filt);
}

inline string upper(const string &s)
{
	string s2;
	for(size_t i=0;i<s.length();i++) {
		s2 += ((unsigned char)toupper(s.c_str()[i]));
	}
	return s2;
}

inline string lower(const string &s)
{
	string s2;
	for(size_t i=0;i<s.length();i++) {
		s2 += ((unsigned char)tolower(s.c_str()[i]));
	}
	return s2;
}

/**
 * ʱ���ֶ�
 * @see #time_add
 */
enum TimeField {Year,Month,Day,Hour,Minute,Second};

/**
 * ʱ�䴦����
 * @param t Դʱ��
 * @param f ʱ���ֶ�
 * @param i �ӵ�ֵ,��Ϊ������ʾ��
 * @return time_t <0ʱ��ʾ������Χ
 */
inline time_t time_add(time_t t,TimeField f,int i)
{
	struct tm curr;
	switch(f) {
	case Second:
		return t+i;
	case Minute:
		return t+i*60;
	case Hour:
		return t+i*60*60;
	case Day:
		return t+i*60*60*24;
	case Month:
		curr = *localtime(&t);
		curr.tm_year += (curr.tm_mon-1+i)/12;
		curr.tm_mon = (curr.tm_mon-1+i) %12+1;
		return mktime(&curr);
	case Year:
		curr = *localtime(&t);
		curr.tm_year+=i;
		return mktime(&curr);
	default:
		return 0;
	}

	return 0;
}

/**
 * unsignedתstring
 * @see #s2u
 */
inline string u2s(const unsigned u)
{
	char sTmp[16] ={0};
	sprintf(sTmp, "%u", u);
	return string(sTmp);
}

/**
 * unsignedתstring(16������ʽ)
 * @see #sx2u
 */
inline string u2sx(const unsigned u)
{
	char sTmp[16] ={0};
	sprintf(sTmp, "%x", u);
	return string(sTmp);
}

/**
 * intתstring
 * @see #s2i
 */
inline string i2s(const int i)
{
	char sTmp[16] ={0};
	sprintf(sTmp, "%d", i);
	return string(sTmp);	
}

/**
 * charתstring
 * @see #s2c
 */
inline string c2s(const char c)
{
	if(c==0) return string("");
	return string()+c;
}

/**
 * binary->string
 * @param b binaryָ��
 * @param len binary����
 * @param block ��ʾ��ʽ<br>
 *                      =0��ʽΪ"00 01 02 ..." <br>
 *                      >0��ʽΪ(��8Ϊ��)"0000h: 00 01 02 03 04 05 06 07 08  09 0a 0b 0c 0d 0e 0f" <br>
 *                                                               "0010h: 10 11 12 13 14 15 16 17 18  19 1a 1b 1c 1d 1e 1f" <br>
 * @return string �õ��ĸ�ʽ��
 * @see #s2b
 */
string b2s(const char *b,size_t len,const unsigned block = 8);

/**
 * time_t->string
 * @param t ʱ��ֵ
 * @param format ��ʾ��ʽ,�μ�dateָ���ʽ
 * @return string �õ��ĸ�ʽ��
 * @s2t
 */
inline string t2s(const time_t t,const string& format="%Y-%m-%d %H:%M:%S")
{
	struct tm curr;
	curr = *localtime(&t);
	char sTmp[1024];
	strftime(sTmp,sizeof(sTmp),format.c_str(),&curr);
	return string(sTmp);
}

/**
 * now->string
 * @param format ��ʾ��ʽ,�μ�dateָ���ʽ
 * @return string �õ��ĸ�ʽ��
 * @see #t2s
 */
inline string now(const string& format="%Y-%m-%d %H:%M:%S")
{
	return t2s(time(0),format);
}


/**
 * stringתunsigned
 * @throw comm_error string��ʽ����unsigned
 * @see #u2s
 */
inline unsigned s2u(const string &s) throw (comm_error)
{
	if(s.length()==0) {
		throw comm_error("s2u: len is 0");
	}
	for (size_t i = 0; i < s.length(); i++){
		if (!isdigit(s.c_str()[i])) {
			throw comm_error(string("s2u: not digits:")+s);
		}
	}

	return strtoul(s.c_str(),NULL,10);
}


/**
 * stringתunsigned (��ʽ����ʱ��ΪĬ��ֵ)
 * @see #u2s
 */
inline unsigned s2u(const string &s,unsigned defaultvalue)
{
	if(s.length()==0) {
		return defaultvalue;
	}
	for (size_t i = 0; i < s.length(); i++){
		if (!isdigit(s.c_str()[i])) {
			return defaultvalue;	
		}
	}

	return strtoul(s.c_str(),NULL,10);
}

/**
 * 16���Ƹ�ʽstringתunsigned,֧�ֵĸ�ʽΪ0x+�ַ�������ֱ��Ϊ�ַ���
 * @throw comm_error string��ʽ����unsigned
 * @see #u2sx
 */
inline unsigned sx2u(const string &s) throw (comm_error)
{
	size_t len = s.length();
	size_t idx=0;
	if(s.c_str()[0] == '0' && s.c_str()[1] == 'x') {
		idx = 2;
		len -= 2;
	}

	if(len==0 || len>8) {
		throw comm_error(string("sx2u: length is invalid:")+u2s(len));
	}
	for (size_t i = idx; i < s.length(); i++){
		if (!isxdigit(s.c_str()[i])) {
			throw comm_error(string("sx2u: not xdigit:")+s);
		}
	}

	return strtoul(s.c_str()+idx,NULL,16);
}

/**
 * 16���Ƹ�ʽstringתunsigned (��ʽ����ʱ��ΪĬ��ֵ)
 * @see #u2sx
 */
inline unsigned sx2u(const string &s,unsigned defaultvalue)
{
	size_t len = s.length();
	size_t idx=0;
	if(s.c_str()[0] == '0' && s.c_str()[1] == 'x') {
		idx = 2;
		len -= 2;
	}

	if(len==0 || len>8) {
		return defaultvalue;
	}
	for (size_t i = idx; i < s.length(); i++){
		if (!isxdigit(s.c_str()[i])) {
			return defaultvalue;
		}
	}

	return strtoul(s.c_str()+idx,NULL,16);
}

/**
 * stringתint
 * @throw comm_error string��ʽ����unsigned
 * @see #i2s
 */
inline int s2i(const string &s) throw (comm_error)
{
	if(s.c_str()[0] == '-') {
		return -1 * s2u(s.c_str()+1);
	}
	else {
		return s2u(s);
	}

	return 0;
}

/**
 * stringתint (��ʽ����ʱ��ΪĬ��ֵ)
 * @see #i2s
 */
inline int s2i(const string &s,int defaultvalue) 
{
	unsigned d = (defaultvalue < 0?-1*defaultvalue:defaultvalue);
	if(s.c_str()[0] == '-') {
		return -1 * s2u(s.c_str()+1,d);
	}
	else {
		return s2u(s,d);
	}

	return 0;
}

/**
 * stringתchar (ȡstring��һλ)
 * @see #c2s
 */
inline char s2c(const string &s)
{
	if(s.length()==0) return 0;
	return s.c_str()[0];
}

/**
 * stringתchar (stringΪ""ʱ����Ĭ��ֵ)
 * @see #c2s
 */
inline char s2c(const string &s,char defaultvalue)
{
	if(s.length()==0) return defaultvalue;
	return s.c_str()[0];
}

/**
 * stringתbinary,֧�ֵĸ�ʽ"00 01 ..."��"0001..."
 * @throw comm_error string��ʽ����
 * @return size_t binary��С
 * @see #b2s
 */
inline size_t s2b(const string &s,char *b,size_t maxlen) throw (comm_error)
{
	if(maxlen==0) return 0;
	if(s.length()==0) return 0;
	char sTmp[4]={0};
	size_t i=0;
	char *p=const_cast<char *>(s.c_str());

	while(*p != '\0') {
		if(!isxdigit(*p)) throw comm_error(string("s2b: format error:")+s);
		sTmp[0] = *p; p++;
		if(!isxdigit(*p)) throw comm_error(string("s2b: format error:")+s);
		sTmp[1] = *p; p++;
		b[i++] = strtoul(sTmp,NULL,16);
		if(i==maxlen) break;
		if(*p == ' ') p++;
	}
	return i;
}

/**
 * stringתtime_t
 * @throw comm_error string��ʽ����
 * @return time_t ����ʱ��
 * @see #t2s
 */
time_t s2t(const string &s,const string& format="%Y-%m-%d %H:%M:%S") throw (comm_error);

// in: s - src string
// out: s - string after trim
// return : head
/**
 * ����splitȥ��string��ͷ,����GetToken<br>
 * ��string s = "ab cd ef", trim(s)����ab��s���Ϊ"cd ed"
 * @param s Դ�ַ���
 * @param split �ָ���
 * @return ͷ
 */
string trim_head(string& s,const string& split=" \t");

}}

#endif //

