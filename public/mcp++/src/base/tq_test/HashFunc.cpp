
#include "HashFunc.h"

static u_int32_t g_iBuildTable32 = 0;
static u_int32_t Table_CRC[256];
static u_int32_t  cnCRC_32 = 0x04C10DB7;

void BuildTable32(u_int32_t aPoly)
{
	if(g_iBuildTable32)	return;
	
	u_int32_t i, j;
	u_int32_t nData;
	u_int32_t nAccum;
	for ( i = 0; i < 256; i++ )
	{
		nData = ( u_int32_t )( i << 24 );
		nAccum = 0;
		for ( j = 0; j < 8; j++ )
		{
			if ( ( nData ^ nAccum ) & 0x80000000 )
				nAccum = ( nAccum << 1 ) ^ aPoly;
			else
				nAccum <<= 1;
			
			nData <<= 1;
		}
		Table_CRC[i] = nAccum;
	}

	g_iBuildTable32 = 1;
}
/*
CRC_32 具有极佳的分布特性和性能
*/
u_int32_t CRC_32(char *data, u_int32_t len)
{
	if(!g_iBuildTable32)
		BuildTable32( cnCRC_32 );

	unsigned char* pdata = (unsigned char*)data;
	u_int32_t i;
	u_int32_t nAccum = 0;
	for ( i = 0; i < len; i++ )
		nAccum = ( nAccum << 8 ) ^ Table_CRC[( nAccum >> 24 ) ^ *pdata++];
	
	return nAccum;
}

//------------------------------------------------------------------------------
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
		  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const u_int16_t *) (d)))
#endif
		
#if !defined (get16bits)
#define get16bits(d) ((((u_int32_t)(((const u_int8_t *)(d))[1])) << 8)\
		                       +(u_int32_t)(((const u_int8_t *)(d))[0]) )
#endif

u_int32_t SuperFastHash(char *data, u_int32_t len) 
{
    u_int32_t hash = len, tmp;
    u_int32_t rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (u_int16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
            hash ^= hash << 16;
            hash ^= data[sizeof (u_int16_t)] << 18;
            hash += hash >> 11;
            break;
        case 2: hash += get16bits (data);
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
        case 1: hash += *data;
            hash ^= hash << 10;
            hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}
//------------------------------------------------------------------------------
u_int32_t RSHash(char *data, u_int32_t len)
{
   u_int32_t b    = 378551;
   u_int32_t a    = 63689;
   u_int32_t hash = 0;

   for(u_int32_t i = 0; i < len; i++)
   {
      hash = hash * a + data[i];
      a    = a * b;
   }

   return hash;
}
/*
256:
	RSHash 641000/s
	SuperFastHash 1202000/s
	CRC_32 549000/s

1024:
	RSHash 161000/s
	SuperFastHash 308000/s
	CRC_32 138000/s
*/
#if 0
#include <sys/time.h>
int main()
{
	timeval t1,t2;
	int span = 0;
	#define CNT	500000
	long long secnum = 0;
	
	char data[1024];
	for(int i=0; i<sizeof(data);i++)
	{
		data[i] = (char)i;
	}
	
	gettimeofday(&t1,NULL);
	for(int i=0;i<CNT;i++)
	{
		RSHash((char*)data, 1024);
	}
	gettimeofday(&t2,NULL);
	
	span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
	secnum = (long long)(CNT/(float)span) * (long long)1000;
	printf("RSHash %lld/s\n",secnum);
	//-------------
	gettimeofday(&t1,NULL);
	for(int i=0;i<CNT;i++)
	{
		SuperFastHash((char*)data, 1024);
	}
	gettimeofday(&t2,NULL);
	span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
	secnum = (long long)(CNT/(float)span) * (long long)1000;
	printf("SuperFastHash %lld/s\n",secnum);
	//-------------
	gettimeofday(&t1,NULL);
	for(int i=0;i<CNT;i++)
	{
		CRC_32((char*)data, 1024);
	}
	gettimeofday(&t2,NULL);
	span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
	secnum = (long long)(CNT/(float)span) * (long long)1000;
	printf("CRC_32 %lld/s\n",secnum);
}
#endif


