
#include "tfc_base_parsepara.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static char __BIN_HEAD_[] = {0x02,0x02,0x02,0xde,0x0};
static char __BIN_TAIL_[] = {0xc8,0xc2,0xc6,0xc9,0x0};

tfc::base::parse_formaterr::parse_formaterr(const string & s):runtime_error(s){}

inline unsigned tfc::base::parse_x2u(const string &s)
{
	size_t len = s.length();
	size_t idx=0;
	if(s.c_str()[0] == '0' && s.c_str()[1] == 'x') {
		idx = 2;
		len -= 2;
	}

	if(len==0 || len>8) {
		throw parse_formaterr(string("CCgiPara::Decode: length is invalid:")+s);
	}
	for (size_t i = idx; i < s.length(); i++){
		if (!isxdigit(s.c_str()[i])) throw parse_formaterr(string("CCgiPara::Decode: not xdigit:")+s);
	}

	return strtoul(s.c_str()+idx,NULL,16);
}

tfc::base::CCgiPara::CCgiPara(){}
tfc::base::CCgiPara::~CCgiPara(){}
// _map内存放的有两种格式,string和binary string
inline bool tfc::base::CCgiPara::isBinaryString(const string &s)
{
	int len = s.length();
	if(len>8) {
		if(s.c_str()[0] == __BIN_HEAD_[0] 
			&& s.c_str()[1] == __BIN_HEAD_[1]
			&& s.c_str()[2] == __BIN_HEAD_[2]
			&& s.c_str()[3] == __BIN_HEAD_[3]
			&& s.c_str()[len-1] == __BIN_TAIL_[3]
			&& s.c_str()[len-2] == __BIN_TAIL_[2]
			&& s.c_str()[len-3] == __BIN_TAIL_[1]
			&& s.c_str()[len-4] == __BIN_TAIL_[0]
			)
		return true;
	}
	return false;
}

// cgi参数格式转换到内部格式(内部格式为string或binary string)
inline string tfc::base::CCgiPara::FormatC2I(const string &s)
{
	string out;
	unsigned char c;
	bool isbin = false;
	char sTmp[4]={0};
	char *p=const_cast<char *>(s.c_str());

	while((unsigned char)*p!=0) {
		if((unsigned char)*p == '%') {
			sTmp[0] = *(p+1); sTmp[1] = *(p+2);
			c = (unsigned char)parse_x2u(sTmp);
//			if(c==0 ||(!isprint(c) && !isspace(c))) {
			if(c==0) {
				isbin = true; p+=3;break; // for check 
			}
			out+=c; p+=3; continue;
		} else {
			out+=(unsigned char)*p;
		}
		p++;
	}

	// 如果是binary string格式
	if(isbin) {
		return string(__BIN_HEAD_)+s+string(__BIN_TAIL_);
	}

	return out;
}

// 内部格式转换到cgi参数格式
// 2004-11-10,改汉字不转码
inline string tfc::base::CCgiPara::FormatI2C(const string &s)
{
	// 如果是binary string格式
	if(isBinaryString(s)) return string(s.c_str(),4,s.length()-8);

	// 如果是string格式
	string out;
	char sTmp[4]={0};
	char *p=const_cast<char *>(s.c_str());
	while(*p!=0) {
		if(*p == '%' || *p == '=' || *p == '\r' || *p == '\n' || *p == '&' || iscntrl(*p)) {
			sprintf(sTmp,"%02x",(unsigned char)*p);
			out+='%';
			out+=sTmp;
		} else {
			out+=(unsigned char)*p;
		}
		p++;
	}

	return out;
	
}

void tfc::base::CCgiPara::Decode(const string & s)throw(parse_formaterr)
{
	string name;
	string value;
	bool find;
	char *p=const_cast<char *>(s.c_str());
	char *p1;
	_paralist.clear();
	while(*p!=0) {
		// find name first
		p1 = p; find = false;
		while(*p1!=0) {
			if(*p1 == '=') {
				find = true;
				break;
			}
			p1++;
		}
		if(!find || p1==p) {
			throw parse_formaterr(string("CCgiPara::Decode:can not find name: ")+s);
		}
		name = FormatC2I(string(p,0,p1-p));

		// find value
		p = ++p1; find = false;
		while(*p1!=0) {
			if(*p1 == '&') {
				find = true;
				break;
			}
			p1++;
		}
		value = FormatC2I(string(p,0,p1-p));
		_paralist[name] = value;
		if(!find) {
			break;
		}
		p = ++p1;
	}
}

string tfc::base::CCgiPara::Encode()
{
	string s;
	map<string,string>::iterator it;
	size_t i=0;
	for(it=_paralist.begin();it!=_paralist.end();it++) {
		if(i>0) s+='&';
		s+=FormatI2C((*it).first);
		s+='=';
		s+=FormatI2C((*it).second);
		i++;
	}
	return s;
}

// binary 到内部格式
string tfc::base::CCgiPara::b2s(const char * b, size_t len)
{
	string out;
	char sTmp[4]={0};

	for(size_t i=0;i<len;i++) {
		if(b[i] == '\0' || b[i] == '%' || b[i] == '=' || b[i] == '\r' || b[i] == '\n' || b[i] == '&' ||(!isprint(b[i]) && !isspace(b[i]))) {
			sprintf(sTmp,"%02x",(unsigned char)b[i]);
			out+='%';
			out+=sTmp;
		} else {
			out+=(unsigned char)b[i];
		}
	}

	return string(__BIN_HEAD_)+out+string(__BIN_TAIL_);
}

string tfc::base::CCgiPara::b2sx(const char * b, size_t len)
{
	string out;
	char sTmp[4]={0};

	for(size_t i=0;i<len;i++) {
		sprintf(sTmp,"%02x",(unsigned char)b[i]);
		out+='%';
		out+=sTmp;
	}

	return string(__BIN_HEAD_)+out+string(__BIN_TAIL_);
}

// 内部格式到bianry
size_t tfc::base::CCgiPara::s2b(const string & s, char * b, size_t maxlen)
{
	size_t len = s.length();
	if(isBinaryString(s)) {
		string s1 = string(s.c_str(),4,len-8);
		unsigned char c;
		char sTmp[4]={0};
		char *p=const_cast<char *>(s1.c_str());
		size_t i=0;
		while((unsigned char)*p!=0) {
			if(i==maxlen) return i;

			if((unsigned char)*p == '%') {
				sTmp[0] = *(p+1); sTmp[1] = *(p+2);
				try {
					c = (unsigned char)parse_x2u(sTmp);
					b[i++] = c; p+=3; continue;
				}
				catch (parse_formaterr& e) {
					b[i++] = '%'; p++; continue;
				}
			} else {
				b[i++]=(unsigned char)*p;
			}
			p++;
		}

		return i;
		
	}

	// 如果是string格式,直接赋值
	len = s.length() > maxlen?maxlen:s.length();
	memcpy(b,s.c_str(),len);
	return len;

}

tfc::base::CSpacePara::CSpacePara()
{
	_split = " ,#\t\r\n";	
}
tfc::base::CSpacePara::CSpacePara(const string & s):_split(s)
{
}
tfc::base::CSpacePara::~CSpacePara(){}

// 2004-11-10,不再支持'\'转义
inline bool tfc::base::CSpacePara::isplit(unsigned char c,unsigned char c2)
{
	char *p=const_cast<char *>(_split.c_str());
	while(*p!=0) {
		if((unsigned char)*p == (unsigned char)c)
			return true;
		p++;
	}
	return false;
}

void tfc::base::CSpacePara::Decode(const string & s)
{
	string value;
	char *p=const_cast<char *>(s.c_str());
	char *p1;
	_paralist.clear();
	while(*p!=0) {
		if(isplit((unsigned char)*p,(unsigned char)*(p+1))) {p++; continue;}
		p1 = p;
		while(*p1!=0) {
			if(isplit((unsigned char)*p1,(unsigned char)*(p1+1))) {
				break;
			}
			p1++;
		}
		value = string(p,0,p1-p);
		_paralist.push_back(value);
		p = p1;
	}
}

void tfc::base::CSpacePara::DecodeStrict(const string & s)
{
	string value;
	char *p=const_cast<char *>(s.c_str());
	char *p1;
	_paralist.clear();
	if(s.length()==0) return;
	while(*p!=0) {
		if(isplit((unsigned char)*p,(unsigned char)*(p+1))) {
			_paralist.push_back("");
			p++; 
			if(*p == 0 && isplit((unsigned char)*(p-1),0)) {
				_paralist.push_back("");
			}
			continue;
		}
		p1 = p;
		while(*p1!=0) {
			if(isplit((unsigned char)*p1,(unsigned char)*(p1+1))) {
				break;
			}
			p1++;
		}
		value = string(p,0,p1-p);
		_paralist.push_back(value);
		if(*p1 == 0) return;
		p = ++p1;
		if(*p == 0 && isplit((unsigned char)*(p-1),0)) {
			_paralist.push_back("");
		}
	}
}


