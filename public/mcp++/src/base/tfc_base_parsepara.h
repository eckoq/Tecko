
#ifndef __TFC_BASE_LIBRARY_PARSEPARA_H__
#define __TFC_BASE_LIBRARY_PARSEPARA_H__

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
using namespace std;
namespace tfc { namespace base{

struct parse_formaterr:public runtime_error { parse_formaterr(const string& s);};

/**
 * CGi类参数解析<br>
 * <p>Description: CGI类参数定义: </p>
 * <p>name1=value1&name2=value2...</p>
 * <p>其中name和value字符串要以base64编码</p>
 * <p>Copyright: Copyright (c) 2004</p>
 * <p>Create: 2004-11-15</p>
 * <p>LastModify: 2004-12-14</p>
 * @author  casper@tencent.com
 * @version  0.4
 * @since version 0.3
 */
class CCgiPara {
public:
	CCgiPara();
	/**
	 * no implementation
	 */
	CCgiPara(const CCgiPara&);
	~CCgiPara();

public:
	/**
	 * 清空内存中的数据
	 */
	void Clear() 
		{_paralist.clear();}
	/**
	 * 解析Cgi参数
	 * @throw parse_formaterr 格式错误
	 * @param s 待解析的字符串
	 */
	void Decode(const string& s) 
		throw(parse_formaterr);
	/**
	 * 打包Cgi参数
	 */
	string Encode();
	/**
	 * 取Cgi参数值,可用作赋值
	 * @param name 参数名
	 * @return 参数值,如果参数不存在,返回string("")
	 */
	string& operator [](const string& name) 
		{ return _paralist[name];}
	/**
	 * 取所有参数对
	 */
	const map<string,string>& GetPairs() const 
		{return _paralist;}

public: 
	/**
	 * 二进制格式打包,编码的字符有0/'%'/'='/'\r'/'\n'
	 * @param b 二进制数据指针
	 * @param len 二进制数据长度
	 * @return 编码为可直接给operator []赋值的string
	 */
	static string b2s(const char *b,size_t len); // 0-"%00" ; '%'-"%25" ; '='-"%3d" ; "\r"-"%0d" ; "\n"-"%0a" ... 
	/**
	 * 二进制格式打包,全部编码,建议使用b2s
	 * @see #b2s
	 * @param b 二进制数据指针
	 * @param len 二进制数据长度
	 * @return 编码为可直接给operator []赋值的string
	 */
	static string b2sx(const char *b,size_t len); // all - "%xx",not recommend
	/**
	 * 二进制格式解包
	 * @param s 从operator []取得的string数据
	 * @param b 二进制数据指针
	 * @param len 二进制数据最大长度
	 * @return 二进制数据长度
	 */
	static size_t s2b(const string &s,char *b,size_t maxlen); 

protected:
	inline static bool isBinaryString(const string &s);
	inline string FormatC2I(const string &s);
	inline string FormatI2C(const string &s);
protected:
	map <string,string> _paralist;

};

/**
 * 分隔参数解析类</p>
 * <p>Description: 以空格或TAB分割的字符串解析</p>
 * <p>Copyright: Copyright (c) 2004</p>
 * <p>Create: 2004-11-15</p>
 * <p>LastModify: 2004-12-14</p>
 * @author  casper@tencent.com
 * @version  0.4
 * @since version 0.3
 */
class CSpacePara {
public:
	CSpacePara();
	/**
	 * @param s 分隔符
	 */
	CSpacePara(const string& s);
	/**
	 * no implementation
	 */
	CSpacePara(const CSpacePara&);
	~CSpacePara();
public:
	/**
	 * 解析,忽略连续的分隔符
	 * @param s 待解析的字符串
	 */
	void Decode(const string& s);
	/**
	 * 严格解析,如果有连续的分隔符,视为中间存在string("")
	 * @param s 待解析的字符串
	 */
	void DecodeStrict(const string& s); //
	/**
	 * 取所有参数
	 */
	const vector<string>& GetPairs() const 
		{return _paralist;}

public: 
	/**
	 * 设置分隔符,默认为" ,#\t\r\n"
	 */
	void SetSplitChar(const string& s=" ,#\t\r\n") 
		{_split=s;}

protected:
	bool isplit(unsigned char c,unsigned char c2);
protected:
	string _split;
	vector<string> _paralist;
};


inline unsigned parse_x2u(const string &s);

}}

#endif //

