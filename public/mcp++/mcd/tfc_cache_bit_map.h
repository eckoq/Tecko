
#ifndef _TFC_CACHE_BIT_MAP_H_
#define _TFC_CACHE_BIT_MAP_H_

//////////////////////////////////////////////////////////////////////////
//
//	author: tomxiao@tencent.com
//	create: 2007-02
//	
//
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>

namespace tfc{namespace cache
{
    typedef enum tagEBitFlag
    {
    	BIT_FLAG_CLEAR = 0x00,
    	BIT_FLAG_SET = 0x01	
    }EBitFlag;

    class CBitMap
    {
    public:
        CBitMap():_pool(NULL), _pool_size(0), _set_count(0){};
        ~CBitMap(){};	

        //初始化 BIT_MAP内存块
        int open(char* pool, bool init, unsigned pool_size);

        int set_bit(unsigned long long pos);
        bool is_bit_set(unsigned long long pos);
        int clear_bit(unsigned long long pos);
        int clear();    //清除所有位
        int dump(std::string dump_file);
        int load_dump(std::string dump_file);
        unsigned long long bit_set_num(){return _set_count;}

        //dump与操作(dump 文件读取到内存)
        int dump_and(std::string dump_file);
        int dump_or(std::string dump_file);

		unsigned long long get_set_count();
        double used_persentage();
    	
    protected:
        void init_pool();
        void init_bit_count();
        int verify_pool();
        

    protected:
    	char *_pool;                        // bitmap头指针
    	unsigned _pool_size;                // bitmap内存快尺寸，以byte为单位
    	unsigned long long _set_count;      // bitmap已置位的位数
    	
    private:
        int _bit_count[256];            //用于数字含1个数的计算
};

//////////////////////////////////////////////////////////////////////////

//设置1
inline int CBitMap::set_bit(unsigned long long pos)
{
    unsigned bytes = pos / 8;
    assert(bytes < _pool_size);

    char mask = 1 << (7 - (pos % 8));
    char *dest = _pool + bytes;

    //bit not set yet, count++, otherwise  count unchanged
    if ( ((*dest) & mask ) == 0x00)
        _set_count++;
            
    *dest = (*dest) | mask;

    return 0;
}

inline bool CBitMap::is_bit_set(unsigned long long pos)
{
    unsigned bytes = pos / 8;
    assert(bytes < _pool_size);

    char mask = 1 << (7 - (pos % 8));
    char *dest = _pool + bytes;

    if ( ((*dest) & mask ) == 0x00)
        return false;
    else
        return true;
}

inline int CBitMap::clear_bit(unsigned long long pos)
{
    unsigned bytes = pos / 8;
    assert(bytes < _pool_size);

    char mask = (1 << (7 - (pos % 8))) ^ 0xFF;
    char *dest = _pool + bytes;

    //bit already set, count--, otherwise  count unchanged
    if ( ((*dest) & mask ) != 0x00)
        _set_count--;
    
    *dest = (*dest) & mask;

    return 0;
}

inline unsigned long long CBitMap::get_set_count()
{
    return _set_count;
}
inline double CBitMap::used_persentage()
{
    return (double)(_set_count / 8) / _pool_size;
}

}}
//////////////////////////////////////////////////////////////////////////
#endif//_TFC_CACHE_BIT_MAP_H_
///:~
