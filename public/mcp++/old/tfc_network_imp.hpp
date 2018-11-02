
#ifndef _TFC_NETWORK_IMP_HPP_
#define _TFC_NETWORK_IMP_HPP_

#include <set>

#include "tfc_sock.hpp"
#include "tfc_simple_sock.hpp"
#include "tfc_network.hpp"

namespace tfc{namespace network{

//////////////////////////////////////////////////////////////////////////

class CNBConn : public Connection
{
public:
	virtual ~CNBConn(){}
	
	virtual CBuffer& Recv();
	virtual int Send(const char* sSendBuf, size_t iSendLen);
	virtual bool SendAvailable();
	virtual int SendCache();
	
	virtual bool IsClosed(){return !_socket.socket_is_ok();}
	virtual int FD(){return _socket.fd();}
	virtual void AttachFD(int fd)
	{
		_socket.attach(fd);
		_socket.set_nonblock();
	}
	
protected:
	net::CSocket _socket;
	CBuffer _cache;
	ptr<CBuffer> _send_cache;

	static const unsigned C_TEMP_BUF_SIZE = 4096;
	static const unsigned C_LOOP_LIMITED = 128;
};

class CNBConnAlloc : public ConnAllocator
{
public:
	virtual ~CNBConnAlloc(){}
	virtual ptr<Connection> AllocateConn(){return ptr<Connection>(new CNBConn());}
};

inline CBuffer& CNBConn::Recv()
{
	//
	//	first, check socket ready
	//

	if (!_socket.socket_is_ok())
		return _cache;

	//
	//	second, read until empty
	//

	for(unsigned i = 0; i < C_LOOP_LIMITED; i++)
	{
		try
		{
			char sBuf[C_TEMP_BUF_SIZE];
			int ret = _socket.receive(sBuf, sizeof(sBuf));
			if (ret > 0)
			{
				_cache.append(sBuf, ret);
			}
			else
			{
				_socket.close();
				throw conn_closed("CNBConn::Recv: conn closed");
			}
		}
		catch (net::socket_intr& ex)
		{
			continue;
		}
		catch (net::socket_again& ex)
		{
			break;
		}
		catch (net::socket_error& ex)
		{
			_socket.close();
			throw conn_closed(std::string("CNBConn::Recv: conn closed") + ex.what());
		}
	}

	return _cache;
}

inline int CNBConn::Send(const char* sSendBuf, size_t iSendLen)
{
	//
	//	first, check if data wait for sending
	//

	if (!_send_cache.IsNil())
	{
		if (_send_cache->size() != 0)
		{
			SendCache();
		}

		if (_send_cache->size() != 0)
		{
			_send_cache->append(sSendBuf, iSendLen);
			return 0;
		}
	}

	//
	//	second, old cache sent or no old cache
	//
	unsigned offset = 0;
	for(unsigned i = 0; i < C_LOOP_LIMITED && offset < iSendLen; i++)
	{
		try
		{
			int ret = _socket.send(sSendBuf + offset, iSendLen - offset, MSG_NOSIGNAL);
			if (ret > 0)
			{
				offset += ret;
			}
		}
		catch (net::socket_intr& ex)
		{
			continue;
		}
		catch (net::socket_again& ex)
		{
			break;
		}
		catch (net::socket_error& ex)
		{
			_socket.close();
			throw conn_closed(std::string("CNBConn::Recv: conn closed") + ex.what());
		}
	}

	//
	//	any left?
	//

	if (offset < iSendLen)
	{
		if (_send_cache.IsNil())
		{
			_send_cache = new CBuffer();
		}
		_send_cache->append(sSendBuf+offset, iSendLen-offset);
	}

	return offset;
}

inline int CNBConn::SendCache()
{
	for(unsigned i = 0; i < C_LOOP_LIMITED; i++)
	{
		try
		{
			//char s[4096];
			CBuffer::iterator it = _send_cache->begin();
			unsigned size = _send_cache->size();// > 4096 ? 4096 : _send_cache->size();
			int ret = _socket.send(it(), size);
			if (ret > 0)
			{
				//	
				_send_cache->skip(ret);
				if (_send_cache->size() > 0)
				{
					continue;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;//	...
			}
		}
		catch (net::socket_intr& ex)
		{
			continue;
		}
		catch (net::socket_again& ex)
		{
			break;
		}
		catch (net::socket_error& ex)
		{
			_socket.close();
			throw conn_closed("CNBConn::SendCache: conn closed");
		}
	}	//	end of for(;;)

	return _send_cache->size();
}

inline bool CNBConn::SendAvailable()
{
	if (_send_cache.IsNil())
		return false;
	
	return _send_cache->size() > 0;
}

//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::network

#endif//_TFC_NETWORK_IMP_HPP_
///:~
