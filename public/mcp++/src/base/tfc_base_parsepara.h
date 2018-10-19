
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
 * CGi���������<br>
 * <p>Description: CGI���������: </p>
 * <p>name1=value1&name2=value2...</p>
 * <p>����name��value�ַ���Ҫ��base64����</p>
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
	 * ����ڴ��е�����
	 */
	void Clear() 
		{_paralist.clear();}
	/**
	 * ����Cgi����
	 * @throw parse_formaterr ��ʽ����
	 * @param s ���������ַ���
	 */
	void Decode(const string& s) 
		throw(parse_formaterr);
	/**
	 * ���Cgi����
	 */
	string Encode();
	/**
	 * ȡCgi����ֵ,��������ֵ
	 * @param name ������
	 * @return ����ֵ,�������������,����string("")
	 */
	string& operator [](const string& name) 
		{ return _paralist[name];}
	/**
	 * ȡ���в�����
	 */
	const map<string,string>& GetPairs() const 
		{return _paralist;}

public: 
	/**
	 * �����Ƹ�ʽ���,������ַ���0/'%'/'='/'\r'/'\n'
	 * @param b ����������ָ��
	 * @param len ���������ݳ���
	 * @return ����Ϊ��ֱ�Ӹ�operator []��ֵ��string
	 */
	static string b2s(const char *b,size_t len); // 0-"%00" ; '%'-"%25" ; '='-"%3d" ; "\r"-"%0d" ; "\n"-"%0a" ... 
	/**
	 * �����Ƹ�ʽ���,ȫ������,����ʹ��b2s
	 * @see #b2s
	 * @param b ����������ָ��
	 * @param len ���������ݳ���
	 * @return ����Ϊ��ֱ�Ӹ�operator []��ֵ��string
	 */
	static string b2sx(const char *b,size_t len); // all - "%xx",not recommend
	/**
	 * �����Ƹ�ʽ���
	 * @param s ��operator []ȡ�õ�string����
	 * @param b ����������ָ��
	 * @param len ������������󳤶�
	 * @return ���������ݳ���
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
 * �ָ�����������</p>
 * <p>Description: �Կո��TAB�ָ���ַ�������</p>
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
	 * @param s �ָ���
	 */
	CSpacePara(const string& s);
	/**
	 * no implementation
	 */
	CSpacePara(const CSpacePara&);
	~CSpacePara();
public:
	/**
	 * ����,���������ķָ���
	 * @param s ���������ַ���
	 */
	void Decode(const string& s);
	/**
	 * �ϸ����,����������ķָ���,��Ϊ�м����string("")
	 * @param s ���������ַ���
	 */
	void DecodeStrict(const string& s); //
	/**
	 * ȡ���в���
	 */
	const vector<string>& GetPairs() const 
		{return _paralist;}

public: 
	/**
	 * ���÷ָ���,Ĭ��Ϊ" ,#\t\r\n"
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

