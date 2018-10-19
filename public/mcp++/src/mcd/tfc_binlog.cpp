#include "tfc_binlog.hpp"

CBinLog::CBinLog()
{
	_maxlog_size = 20000000;
	_maxlog_num = 8;
	strcpy(_log_basename,"./mcdbin_");
	_write_fp = NULL;
	
	_read_fp = NULL;
	_readidx = _maxlog_num;

	szTmpBuff = new char[MAX_BINLOG_ITEM_LEN];
}

CBinLog::~CBinLog()
{
	if (_write_fp) 
	{
		fclose(_write_fp);
		_write_fp = NULL;
	}	
	if (_read_fp) 
	{
		fclose(_read_fp);
		_read_fp = NULL;
	}	
	delete szTmpBuff;
}

int CBinLog::Init(char *sPLogBaseName, long lPMaxLogSize, int iPMaxLogNum)
{
	if (_write_fp) 
	{
		fclose(_write_fp);
		_write_fp = NULL;
	}	
	if (_read_fp) 
	{
		fclose(_read_fp);
		_read_fp = NULL;
	}
	
	memset(_log_basename, 0, sizeof(_log_basename));
	strncpy(_log_basename, sPLogBaseName, sizeof(_log_basename)-1);

	_maxlog_num = iPMaxLogNum;
	_maxlog_size = lPMaxLogSize;	
	return 0;
}

int CBinLog::ClearAllBinLog()
{
	char sLogFileName[300];
	for (int i=_maxlog_num-1; i>=0; i--)
	{
	        if (i == 0)
	            sprintf(sLogFileName,"%s.log", _log_basename);
	        else
	            sprintf(sLogFileName,"%s%d.log", _log_basename, i);
		
		if (access(sLogFileName, F_OK) != 0)
		{
			continue;
		}
		remove(sLogFileName) ;
	}

	if (_write_fp)
	{
		fclose(_write_fp);
		_write_fp = NULL;	
	}
	if (_read_fp)
	{
		fclose(_read_fp);
		_read_fp = NULL;	
	}	
	return 0;
}

int CBinLog::SetReadRecordStartTime(int starttime/*=-1*/)
{
	if (_read_fp)
	{
		fclose(_read_fp);
		_read_fp = NULL;
	}

	if (starttime == -1)
	{
		_readidx = _maxlog_num;
		return 0;
	}
	
	char sLogFileName[300];
	int tLogTime;
	int len;
	char buff[65535];
	FILE *_read_fp_mybe = NULL;
	int maybeidx = 0;
	
	for (_readidx = 0; _readidx < _maxlog_num; _readidx++)
	{
		if (_readidx == 0)
			sprintf(sLogFileName,"%s.log", _log_basename);
		else
			sprintf(sLogFileName,"%s%d.log", _log_basename, _readidx);

		if(access(sLogFileName, F_OK) != 0)
		{
			continue;
		}
		
		_read_fp = fopen(sLogFileName,"r+");	
		if (!_read_fp)
		{
			return -1;
		}
		fread(&tLogTime,sizeof(int),1,_read_fp);
		
		//�ض��ڱ��ļ���
		if (tLogTime < starttime)
		{
			fread(&len,sizeof(int),1,_read_fp);
			if (len > (int)sizeof(buff))
			{
				fclose(_read_fp); 
				_read_fp = NULL;
				return -1;
			}
			fread(buff,len,1,_read_fp);

			//�����ж�
			do
			{
				fread(&tLogTime,sizeof(int),1,_read_fp);
				fread(&len,sizeof(int),1,_read_fp);
				if (len > (int)sizeof(buff))
				{
					fclose(_read_fp); 
					_read_fp = NULL;				
					return -1;
				}				
				fread(buff,len,1,_read_fp);
			}while((tLogTime<starttime) && (!feof(_read_fp)));

			//�ҵ���
			if (tLogTime >= starttime)
			{
				//�ֽڻ���
				fseek(_read_fp,-8-len,SEEK_CUR);
				break;
			}

			if (feof(_read_fp))
			{
				fclose(_read_fp);
				_read_fp = NULL;
				if (_read_fp_mybe)
				{
					_read_fp = _read_fp_mybe;				
					_readidx = maybeidx;
				}
			}

			break;
		}	
		//�ض����ڱ��ļ���
		else if (tLogTime > starttime)
		{
			fclose(_read_fp);
			_read_fp = NULL;
		}
		//�����ڱ��ļ�����һ����		
		else
		{
			if (_read_fp_mybe) fclose(_read_fp_mybe);

			//����4�ֽ�
			fseek(_read_fp,-4,SEEK_CUR);
			_read_fp_mybe = _read_fp;
			_read_fp = NULL;
			maybeidx = _readidx;
		}
	}

	if (_read_fp)
	{
		return 0;
	}
	else if(_read_fp_mybe)
	{
		_read_fp = _read_fp_mybe;
		_readidx = maybeidx;
		return 0;
	}
	
	return -1;

}

#if 0
int CBinLog::ReadBinLogBegin()
{
	if ( _read_fp ) {
		fclose(_read_fp);
		_read_fp = NULL;
	}

	_read_fp = fopen(_log_basename, "r+");
	if ( !_read_fp ) {
		return -1;
	}

	return 0;
}
// Returns > 0, Success and just the bytes of binlog item.
// 0 File end.
// -1 Error, -2 buffer not enough.
int CBinLog::ReadRecordFromBinLog(char* buff, int maxsize)
{
	int tLogTime = 0;
	int len = 0;

	if ( !_read_fp ) {
		return -1;
	}

	if ( feof(_read_fp) ) {
		return 0;
	}

	if ( fread(&tLogTime, 1, sizeof(int), _read_fp) != sizeof(int) ) {
		if ( feof(_read_fp) ) {
			return 0;
		} else {
			return -1;
		}
	}

	if ( fread(&len,1,sizeof(int),_read_fp) != sizeof(int) ) {
		if ( feof(_read_fp) ) {
			return 0;
		} else {
			return -1;
		}
	}

	if (len > maxsize)
	{
		return -2;
	}

	if ( fread(buff, 1, len, _read_fp) != len ) {
		if ( feof(_read_fp) ) {
			return 0;
		} else {
			return -1;
		}
	}

	return len;
}
void CBinLog::ReadBinLogEnd()
{
	if ( _read_fp ) {
		fclose(_read_fp);
		_read_fp = NULL;
	}
}
#endif

//���س���<0ʱ����,=0����Բ�����
int CBinLog::ReadRecordFromBinLog(char* buff, int maxsize)
{
	char sLogFileName[300];	
	int tLogTime = 0;
	int len = 0;

	//��λ��һ��_read_fp
	if(!_read_fp || feof(_read_fp))
	{
		if (_read_fp)
		{
			fclose(_read_fp);
			_read_fp = NULL;
		}
		
		//����һ���ļ�
		do
		{
			_readidx--;
			if (_readidx == 0)
				sprintf(sLogFileName,"%s.log", _log_basename);
			else
				sprintf(sLogFileName,"%s%d.log", _log_basename, _readidx);
		}while((_readidx>=0) && (access(sLogFileName, F_OK)!=0));

		//������
		if (_readidx < 0)
		{
			return -1;
		}
		_read_fp = fopen(sLogFileName,"r+");		
	}
	
	if (!_read_fp)
	{
		return -2;
	}

	//ֻ��readһ�β�֪��feof...
	fread(&tLogTime,sizeof(int),1,_read_fp);
	fread(&len,sizeof(int),1,_read_fp);

	if (len > maxsize)
	{
		fclose(_read_fp);
		_read_fp = NULL;
		return -3;
	}
	if (feof(_read_fp))
	{
		//���ļ�����,������һ���ļ�
		return ReadRecordFromBinLog(buff,maxsize);
	}
	
	int ret = fread(buff,len,1,_read_fp);	
	return ret;
}

#if 0
int CBinLog::WriteToBinLog(char *buff, int len)
{
	if (!buff || len<=0 || len > (MAX_BINLOG_ITEM_LEN - sizeof(int) - sizeof(int)))
	{
		return -1;
	}
	
	if (_write_fp)
	{
		if (access(_log_basename, F_OK) != 0)
		{
			fclose(_write_fp);
			_write_fp = NULL;
		}
	}

	if (!_write_fp)
	{	    
		_write_fp = fopen(_log_basename,"a+");
		if (!_write_fp)
		{
			return -1;
		}
	}

	struct stat stStat;
	if(stat(_log_basename, &stStat) < 0)
    {
    	fclose(_write_fp);
		_write_fp = NULL;
        return -1;
    }

    if (stStat.st_size >= _maxlog_size)
    {
    	fclose(_write_fp);
		_write_fp = NULL;
        return -1;
    }

	int tNow = time(NULL);
	
	memcpy(szTmpBuff,&tNow,sizeof(int));
	memcpy(szTmpBuff+sizeof(int),&len,sizeof(int));
	memcpy(szTmpBuff+sizeof(int)+sizeof(int),buff,len);
	
	if ( fwrite(szTmpBuff,len+sizeof(int)+sizeof(int),1,_write_fp) != 1 ) {
		return -1;
	}

	return 0;
}
#endif
int CBinLog::WriteToBinLog(char* buff, int len)
{
#ifdef _TDC_DISKCACHE_
	if (!buff || len<=0 || len > (int)(MAX_BINLOG_ITEM_LEN - sizeof(int) - sizeof(int)))
	{
		return -1;
	}
#else
	if (!buff || len<=0 || len > MAX_BINLOG_ITEM_LEN)
	{
		return -1;
	}
#endif

    	char sLogFileName[300];
  	sprintf(sLogFileName,"%s.log", _log_basename);
	if (_write_fp)
	{
		if (access(sLogFileName, F_OK) != 0)
		{
			fclose(_write_fp);
			_write_fp = NULL;
		}	
	}
			
	if (!_write_fp)
	{	    
		_write_fp = fopen(sLogFileName,"a+");
		if (!_write_fp)
		{
			return -1;
		}
	}

	int tNow = time(NULL);
	
	memcpy(szTmpBuff,&tNow,sizeof(int));
	memcpy(szTmpBuff+sizeof(int),&len,sizeof(int));
	memcpy(szTmpBuff+sizeof(int)+sizeof(int),buff,len);
	
	fwrite(szTmpBuff,len+sizeof(int)+sizeof(int),1,_write_fp);

#ifndef _NO_SYNC_

#ifndef _TDC_DISKCACHE_
	fflush(_write_fp);
#endif

#endif	

	return ShiftFiles();
}

int CBinLog::ShiftFiles()
{
   struct stat stStat;
    char sLogFileName[300];
    char sNewLogFileName[300];
    int i;

	sprintf(sLogFileName,"%s.log", _log_basename);
    if(stat(sLogFileName, &stStat) < 0)
    {
        return -1;
    }

    if (stStat.st_size < _maxlog_size)
    {
        return 0;
    }

	if (_write_fp) 
	{
		fclose(_write_fp);
		_write_fp = NULL;
	}	
	
	//last file delete
    sprintf(sLogFileName,"%s%d.log", _log_basename, _maxlog_num-1);
    if (access(sLogFileName, F_OK) == 0)
    {
        if (remove(sLogFileName) < 0 )
        {
            return -1;
        }
    }

    for(i = _maxlog_num-2; i >= 0; i--)
    {
        if (i == 0)
            sprintf(sLogFileName,"%s.log", _log_basename);
        else
            sprintf(sLogFileName,"%s%d.log", _log_basename, i);
            
        if (access(sLogFileName, F_OK) == 0)
        {
            sprintf(sNewLogFileName,"%s%d.log", _log_basename, i+1);
            if (rename(sLogFileName,sNewLogFileName) < 0 )
            {
                return -1;
            }
        }
    }
    return 0;
}

