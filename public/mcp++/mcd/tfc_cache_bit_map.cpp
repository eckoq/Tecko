
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tfc_cache_bit_map.h"

using namespace std;
using namespace tfc::cache;

int CBitMap::open(char* pool, bool init, unsigned pool_size)
{
    int ret = 0;

    _pool = pool;
    _pool_size = pool_size;

    init_bit_count();

    if (init)
    {
    	init_pool();
    }
    else
    {
    	if ((ret = verify_pool()) != 0) 
    	{
    		return ret;
    	}
    }

    return 0;	
}

void CBitMap::init_bit_count()
{
    for(int i=0; i<256; i++)
    {
        int n=0;
        if((i&0x01)!=0)
            n++;
        if((i&0x02)!=0)
            n++;
        if((i&0x04)!=0)
            n++;
        if((i&0x08)!=0)
            n++;
        if((i&0x10)!=0)
            n++;
        if((i&0x20)!=0)
            n++;
        if((i&0x40)!=0)
            n++;
        if((i&0x80)!=0)
            n++;
        *(_bit_count+i)=n;
    }    
}

void CBitMap::init_pool()
{
    // 把内存块置为0	
    memset(_pool, 0, _pool_size);
    return;
}

int CBitMap::verify_pool()
{
    // 计算_set_count的值
    for(unsigned i=0; i<_pool_size; i++)
    {
        unsigned char a=*(_pool+i);
        _set_count += _bit_count[unsigned(a)];
    }

    return 0;
}

int CBitMap::clear()
{
    memset(_pool, 0, _pool_size);
    _set_count=0;
    return 0;
}

int CBitMap::dump(std::string dump_file)
{
    int ret;
    int fd=::open(dump_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 666);
    if(fd<0)
        return -1;

    unsigned bytes_left = _pool_size;
    unsigned bytes_wrote = 0;
    while(bytes_left>0)
    {
        if(bytes_left>4096)
        {
            ret = write(fd, _pool+bytes_wrote, 4096);
            if(ret !=4096)
                return -1;                
        }
        else
        {
            ret = write(fd, _pool+bytes_wrote, bytes_left);
            if(ret != (int)bytes_left)
                return -1;                
        }

        bytes_left-=ret;
        bytes_wrote+=ret;
    }
    close(fd);
    return 0;
}

//没有修改 _set_count!
int CBitMap::load_dump(std::string dump_file)
{
    int ret;
    int fd=::open(dump_file.c_str(), O_RDONLY);
    if(fd<0)
        return -1;

    struct stat statbuf;
    if(fstat(fd, &statbuf)!=0)
    	return -1;
    	
    //unsigned bytes_left = _pool_size;
    unsigned bytes_left = statbuf.st_size;
    unsigned bytes_read = 0;

    while(bytes_left>0)
    {
        if(bytes_left>4096)
        {
            ret = read(fd, _pool+bytes_read, 4096);
            if(ret !=4096)
                return -1;                
        }
        else
        {
            ret = read(fd, _pool+bytes_read, bytes_left);
            if(ret != (int)bytes_left)
                return -1;                
        }

        bytes_left-=ret;
        bytes_read+=ret;
    }
    close(fd);
    return 0;
}

//没有修改 _set_count!
int CBitMap::dump_and(std::string dump_file)
    {
        int ret;
        char buffer[4096];
        
        int fd=::open(dump_file.c_str(), O_RDONLY);
        if(fd<0)
            return -1;

	struct stat statbuf;
	if(fstat(fd, &statbuf)!=0)
	return -1;
	
	//unsigned bytes_left = _pool_size;
	unsigned bytes_left = statbuf.st_size;
        unsigned bytes_read = 0;
    
        while(bytes_left>0)
        {
            if(bytes_left>4096)
            {
                ret = read(fd, buffer, 4096);
                if(ret !=4096)
                    return -1;                
            }
            else
            {
                ret = read(fd, buffer, bytes_left);
                if(ret != (int)bytes_left)
                    return -1;                
            }
    
            for(int i=0; i<ret; i++)
            {
                *(_pool+bytes_read+i) &= buffer[i];
            }

            bytes_left-=ret;
            bytes_read+=ret;
        }
        close(fd);
        return 0;
    }

//没有修改 _set_count!
int CBitMap::dump_or(std::string dump_file)
{
    int ret;
    char buffer[4096];
    
    int fd=::open(dump_file.c_str(), O_RDONLY);
    if(fd<0)
        return -1;

    struct stat statbuf;
    if(fstat(fd, &statbuf)!=0)
    	return -1;
    	
    //unsigned bytes_left = _pool_size;
    unsigned bytes_left = statbuf.st_size;
    unsigned bytes_read = 0;

    while(bytes_left>0)
    {
        if(bytes_left>4096)
        {
            ret = read(fd, buffer, 4096);
            if(ret !=4096)
                return -1;                
        }
        else
        {
            ret = read(fd, buffer, bytes_left);
            if(ret != (int)bytes_left)
                return -1;                
        }

        for(int i=0; i<ret; i++)
        {
            *(_pool+bytes_read+i) |= buffer[i];
        }

        bytes_left-=ret;
        bytes_read+=ret;
    }
    close(fd);
    return 0;
}



