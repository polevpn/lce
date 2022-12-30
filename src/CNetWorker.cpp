#include "CNetWorker.h"

namespace lce
{

	int CNetWorker::init(uint32_t dwMaxClient)
	{
		m_vecClients.resize(dwMaxClient * FD_TIMES, 0);

		if (m_oEvent.init(dwMaxClient * FD_TIMES) < 0)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
			return -1;
		}

		m_queConn.init(dwMaxClient);

		m_iEventFd = eventfd(0, EFD_NONBLOCK);

		if (m_iEventFd == -1)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,init worker errno:%d error:%s", __FILE__, __LINE__, errno, strerror(errno));
			return -1;
		}

		if (m_oEvent.addFdEvent(m_iEventFd, CEvent::EV_READ, std::bind(&CNetWorker::onEvent, this, std::placeholders::_1, std::placeholders::_2), NULL) != 0)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,init worker error:%s", __FILE__, __LINE__, m_oEvent.getErrorMsg());
			return -1;
		}

		if (init() < 0)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,init worker error", __FILE__, __LINE__);
			return -1;
		}

		return 0;
	}

	int CNetWorker::createClient(int iPkgType /* = PKG_RAW */, uint32_t dwInitRecvBufLen /* =10240 */, uint32_t dwMaxRecvBufLen /* =102400 */, uint32_t dwInitSendBufLen /* =102400 */, uint32_t dwMaxSendBufLen /* =1024000 */)
	{
		StServerInfo *pstServerInfo = new StServerInfo;

		pstServerInfo->sIp = "";
		pstServerInfo->wPort = 0;

		pstServerInfo->iType = CONN_TCP;

		pstServerInfo->dwInitRecvBufLen = dwInitRecvBufLen;
		pstServerInfo->dwMaxRecvBufLen = dwMaxRecvBufLen;

		pstServerInfo->dwInitSendBufLen = dwInitSendBufLen;
		pstServerInfo->dwMaxSendBufLen = dwMaxSendBufLen;

		switch (iPkgType)
		{
		case PKG_RAW:
			pstServerInfo->iPkgType = iPkgType;
			pstServerInfo->pPackageFilter = new CRawPackageFilter;
			break;
		case PKG_HTTP:
			pstServerInfo->iPkgType = iPkgType;
			pstServerInfo->pPackageFilter = new CHttpPackageFilter;
			break;
		case PKG_H2ST3:
			pstServerInfo->iPkgType = iPkgType;
			pstServerInfo->pPackageFilter = new CH2ShortT3PackageFilter;
			break;
		case PKG_H2LT3:
			pstServerInfo->iPkgType = iPkgType;
			pstServerInfo->pPackageFilter = new CH2T3PackageFilter;
			break;
		case PKG_EXT:
			pstServerInfo->iPkgType = iPkgType;
			pstServerInfo->pPackageFilter = NULL;
			break;
		default:
			pstServerInfo->iPkgType = PKG_RAW;
			pstServerInfo->pPackageFilter = new CRawPackageFilter;
		}

		pstServerInfo->iSrvId = START_SRV_ID + m_mapServers.size();
		pstServerInfo->iFd = 0;
		m_mapServers[pstServerInfo->iSrvId] = pstServerInfo;

		return pstServerInfo->iSrvId;
	}

	int CNetWorker::setPkgFilter(int iSrvId, CPackageFilter *pPkgFilter)
	{
		if (pPkgFilter == NULL)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,pkgfilter pointer is null", __FILE__, __LINE__);
			return -1;
		}

		map<uint32_t, StServerInfo *>::iterator it = m_mapServers.find(iSrvId);

		if (it == m_mapServers.end())
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,iSrvId error", __FILE__, __LINE__);
			return -1;
		}

		it->second->pPackageFilter = pPkgFilter;

		return 0;
	}

	int CNetWorker::connect(int iSrvId, const string &sIp, uint16_t wPort, void *pData)
	{

		map<uint32_t, StServerInfo *>::iterator it = m_mapServers.find(iSrvId);

		if (it == m_mapServers.end())
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,iSrvId error", __FILE__, __LINE__);
			return -1;
		}

		StServerInfo *pstServerInfo = it->second;

		if (pstServerInfo->iType == CONN_TCP)
		{

			StClientInfo *pstClientInfo = new StClientInfo;
			pstClientInfo->iFd = lce::CreateTcpSock();
			pstClientInfo->pstServerInfo = pstServerInfo;
			pstClientInfo->ddwBeginTime = lce::GetTickCount();

			if (pstClientInfo->iFd < 0)
			{
				snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
				delete pstClientInfo;
				pstClientInfo = NULL;
				return -1;
			}

			lce::SetReUseAddr(pstClientInfo->iFd);
			lce::SetNoneBlock(pstClientInfo->iFd);

			pstClientInfo->stClientAddr.sin_family = AF_INET;
			pstClientInfo->stClientAddr.sin_port = htons(wPort);
			pstClientInfo->stClientAddr.sin_addr.s_addr = inet_addr(sIp.c_str());
			memset(&(pstClientInfo->stClientAddr.sin_zero), 0, 8);

			int iRet = lce::Connect(pstClientInfo->iFd, sIp, wPort);

			if (iRet != -1)
			{
				StSession stSession;
				stSession.ddwBeginTime = pstClientInfo->ddwBeginTime;
				stSession.iFd = pstClientInfo->iFd;
				stSession.iSvrId = pstServerInfo->iSrvId;
				stSession.stClientAddr = pstClientInfo->stClientAddr;
				m_vecClients[pstClientInfo->iFd] = pstClientInfo;
				onConnect(stSession, true, pData);

				if (isClose(pstClientInfo->iFd))
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,connect error maybe client have closed", __FILE__, __LINE__);
					return -1;
				}
				return pstClientInfo->iFd;
			}
			else
			{

				if (errno == EINPROGRESS)
				{
					if (m_oEvent.addFdEvent(pstClientInfo->iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onTcpConnect, this, std::placeholders::_1, std::placeholders::_2), pData) != 0)
					{
						lce::Close(pstClientInfo->iFd);
						delete pstClientInfo;
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
						return -1;
					}
					m_vecClients[pstClientInfo->iFd] = pstClientInfo;
					return pstClientInfo->iFd;
				}
				else
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
					lce::Close(pstClientInfo->iFd);
					delete pstClientInfo;
					pstClientInfo = NULL;
					return -1;
				}
			}
			return -1;
		}
		else
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,server iType error", __FILE__, __LINE__);
			return -1;
		}
		return -1;
	}

	void CNetWorker::onTcpConnect(int iFd, void *pData)
	{

		if (isClose(iFd))
		{
			return;
		}

		StClientInfo *pstClientInfo = m_vecClients[iFd];

		m_oEvent.delFdEvent(iFd, CEvent::EV_WRITE);

		StServerInfo *pstServerInfo = pstClientInfo->pstServerInfo;

		int error;
		socklen_t ilen = sizeof(int);
		getsockopt(iFd, SOL_SOCKET, SO_ERROR, &error, &ilen);

		StSession stSession;
		stSession.ddwBeginTime = pstClientInfo->ddwBeginTime;
		stSession.iFd = iFd;
		stSession.iSvrId = pstServerInfo->iSrvId;
		stSession.stClientAddr = pstClientInfo->stClientAddr;

		if (error == 0)
		{
			onConnect(stSession, true, pData);

			if (!isClose(iFd))
			{

				if (m_oEvent.addFdEvent(pstClientInfo->iFd, CEvent::EV_READ, std::bind(&CNetWorker::onTcpRead, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
				{
					close(iFd);
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
					onError(stSession, m_szErrMsg, ERR_OP_EVENT);
				}
			}
		}
		else
		{
			onConnect(stSession, false, pData);
			close(iFd);
		}
	}

	int CNetWorker::run()
	{
		return m_oEvent.run();
	}

	int CNetWorker::watch(int iFd, void *pData)
	{
		StClientInfo *pstClientInfo = (StClientInfo *)pData;

		if (!m_queConn.enque(pstClientInfo))
		{
			return -1;
		}

		uint64_t ddwNum = 1;
		ssize_t dwSize = ::write(m_iEventFd, &ddwNum, sizeof(uint64_t));

		if (dwSize < 0)
		{
			return -1;
		}

		return 0;
	}

	void CNetWorker::onEvent(int iFd, void *pData)
	{
		uint64_t ddwNum = 0;
		ssize_t dwSize = ::read(m_iEventFd, &ddwNum, sizeof(uint64_t));

		if (dwSize < 0)
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", "onEvent read fail");
		}

		while (!m_queConn.empty())
		{
			StClientInfo *pstClientInfo = NULL;
			m_queConn.deque(pstClientInfo);

			if (pstClientInfo != NULL)
			{
				m_vecClients[pstClientInfo->iFd] = pstClientInfo;

				StSession stSession;
				stSession.ddwBeginTime = pstClientInfo->ddwBeginTime;
				stSession.iFd = iFd;
				stSession.iSvrId = pstClientInfo->pstServerInfo->iSrvId;
				stSession.stClientAddr = pstClientInfo->stClientAddr;

				onConnect(stSession, true, pstClientInfo);

				if (isClose(pstClientInfo->iFd))
					continue;

				if (m_oEvent.addFdEvent(pstClientInfo->iFd, CEvent::EV_READ, std::bind(&CNetWorker::onTcpRead, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
				{
					close(iFd);
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
					onError(stSession, m_szErrMsg, ERR_OP_EVENT);
				}
			}
		}
	}

	void CNetWorker::onTcpRead(int iFd, void *pData)
	{

		StClientInfo *pstClientInfo = m_vecClients[iFd];

		StSession stSession;
		stSession.ddwBeginTime = pstClientInfo->ddwBeginTime;
		stSession.iFd = iFd;
		stSession.iSvrId = pstClientInfo->pstServerInfo->iSrvId;
		stSession.stClientAddr = pstClientInfo->stClientAddr;

		if (pstClientInfo->pSocketRecvBuf == NULL)
		{
			pstClientInfo->pSocketRecvBuf = new CSocketBuf(pstClientInfo->pstServerInfo->dwInitRecvBufLen, pstClientInfo->pstServerInfo->dwMaxRecvBufLen);
		}

		int iSize = 0;

		if (pstClientInfo->pSocketRecvBuf->getFreeSize() == 0)
		{
			pstClientInfo->pSocketRecvBuf->addFreeBuf();
		}

		if (pstClientInfo->pSocketRecvBuf->getFreeSize() == 0)
		{
			// print error no buf size
			close(iFd);
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "onTcpRead %s,%d,socket buf no memory", __FILE__, __LINE__);
			onError(stSession, m_szErrMsg, ERR_NO_BUFFER);
			return;
		}

		iSize = ::recv(iFd, pstClientInfo->pSocketRecvBuf->getFreeBuf(), pstClientInfo->pSocketRecvBuf->getFreeSize(), 0);

		if (iSize > 0)
		{
			pstClientInfo->pSocketRecvBuf->addData(iSize);

			int iWholePkgFlag = 0;

			int iRealPkgLen = 0;
			int iPkgLen = 0;

			if (pstClientInfo->pstServerInfo->pPackageFilter == NULL)
			{
				close(iFd);
				snprintf(m_szErrMsg, sizeof(m_szErrMsg), "onTcpRead %s,%d,package filter is null", __FILE__, __LINE__);
				onError(stSession, m_szErrMsg, ERR_PKG_FILTER);
				return;
			}

			while ((iWholePkgFlag = pstClientInfo->pstServerInfo->pPackageFilter->isWholePkg(pstClientInfo->pSocketRecvBuf->getData(), pstClientInfo->pSocketRecvBuf->getSize(), iRealPkgLen, iPkgLen)) == 0)
			{
				onRead(stSession, pstClientInfo->pSocketRecvBuf->getData(), iPkgLen);
				if (isClose(iFd))
					break;
				pstClientInfo->pSocketRecvBuf->removeData(iPkgLen);
			}
			if (-2 == iWholePkgFlag) // �Ƿ����ݰ�
			{
				close(iFd); // �Ƿ����ݰ�ʱ���ر�����
				snprintf(m_szErrMsg, sizeof(m_szErrMsg), "onTcpRead %s,%d,package invalid", __FILE__, __LINE__);
				onError(stSession, m_szErrMsg, ERR_INVALID_PACKAGE);
			}
		}
		else if (iSize == 0)
		{
			onClose(stSession);
			close(iFd);
		}
		else
		{
			if (errno == 104) // Connection reset by peer
			{
				onClose(stSession);
				close(iFd);
			}
			else if (errno == EAGAIN || errno == EINTR) // ��������������IO��������������ر�����
			{
				snprintf(m_szErrMsg, sizeof(m_szErrMsg), "onTcpRead EAGAIN or EINTR %s,%d,errno=%d,msg=%s", __FILE__, __LINE__, errno, strerror(errno));
				onError(stSession, m_szErrMsg, ERR_NOT_READY);
			}
			else
			{
				snprintf(m_szErrMsg, sizeof(m_szErrMsg), "onTcpRead %s,%d,errno=%d,msg=%s", __FILE__, __LINE__, errno, strerror(errno));
				close(iFd);
				onError(stSession, m_szErrMsg, ERR_SOCKET);
			}
		}
	}

	int CNetWorker::close(const StSession &stSession)
	{
		return close(stSession.iFd);
	}

	int CNetWorker::close(int iFd)
	{
		if (m_vecClients[iFd] != NULL)
		{
			delete m_vecClients[iFd];
			m_vecClients[iFd] = NULL;
			m_oEvent.delFdEvent(iFd, CEvent::EV_READ | CEvent::EV_WRITE);
			return lce::Close(iFd);
		}
		return 0;
	}

	int CNetWorker::write(const StSession &stSession, const char *pszData, const int iSize, bool bClose)
	{

		if (isClose(stSession.iFd))
		{
			snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error maybe client have closed", __FILE__, __LINE__);
			return -1;
		}

		StClientInfo *pstClientInfo = m_vecClients[stSession.iFd];

		StServerInfo *pstServerInfo = pstClientInfo->pstServerInfo;

		pstClientInfo->bNeedClose = bClose;
		int iSendBufSize = 0;
		int iSendSize = 0;

		if (pstClientInfo->pSocketSendBuf == NULL || pstClientInfo->pSocketSendBuf->getSize() == 0)
		{
			iSendSize = lce::Send(stSession.iFd, pszData, iSize);

			if (iSendSize > 0)
			{
				if (iSendSize < iSize)
				{
					if (pstClientInfo->pSocketSendBuf == NULL)
					{
						pstClientInfo->pSocketSendBuf = new CSocketBuf(pstServerInfo->dwInitSendBufLen, pstServerInfo->dwMaxSendBufLen);
					}
					if (!pstClientInfo->pSocketSendBuf->addData(pszData + iSendSize, iSize - iSendSize))
					{
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error buffer less than data", __FILE__, __LINE__);
						return -1;
					}
					if (m_oEvent.addFdEvent(stSession.iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onWrite, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
					{
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
						return -1;
					}
				}
				else
				{
					if (bClose)
					{
						close(stSession.iFd);
					}
					else
					{
						m_oEvent.delFdEvent(stSession.iFd, CEvent::EV_WRITE);
					}
				}
			}
			else
			{
				if (errno == EAGAIN || errno == EINTR)
				{
					if (pstClientInfo->pSocketSendBuf == NULL)
					{
						pstClientInfo->pSocketSendBuf = new CSocketBuf(pstServerInfo->dwInitSendBufLen, pstServerInfo->dwMaxSendBufLen);
					}
					if (!pstClientInfo->pSocketSendBuf->addData(pszData, iSize))
					{
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error buffer less than data", __FILE__, __LINE__);
						return -1;
					}
					if (m_oEvent.addFdEvent(stSession.iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onWrite, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
					{
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
						return -1;
					}
				}
				else
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
					return -1;
				}
			}
		}
		else if ((iSendBufSize = pstClientInfo->pSocketSendBuf->getSize()) > 0)
		{

			int iSendSize = lce::Send(stSession.iFd, pstClientInfo->pSocketSendBuf->getData(), iSendBufSize);
			if (iSendSize > 0)
			{
				pstClientInfo->pSocketSendBuf->removeData(iSendSize);
				iSendBufSize -= iSendSize;
			}
			else
			{
				if (errno != EAGAIN && errno != EINTR)
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
					return -1;
				}
			}

			if (iSendBufSize > 0)
			{
				if (!pstClientInfo->pSocketSendBuf->addData(pszData, iSize))
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error buffer less than data", __FILE__, __LINE__);
					return -1;
				}

				if (m_oEvent.addFdEvent(stSession.iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onWrite, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
					return -1;
				}
			}
			else
			{
				int iSendSize = lce::Send(stSession.iFd, pszData, iSize);
				if (iSendSize > 0)
				{
					if (iSendSize < iSize)
					{
						if (!pstClientInfo->pSocketSendBuf->addData(pszData + iSendSize, iSize - iSendSize))
						{
							snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error buffer less than data", __FILE__, __LINE__);
							return -1;
						}

						if (m_oEvent.addFdEvent(stSession.iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onWrite, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
						{
							snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
							return -1;
						}
					}
					else
					{
						if (bClose)
						{
							close(stSession.iFd);
						}
						else
						{
							m_oEvent.delFdEvent(stSession.iFd, CEvent::EV_WRITE);
						}
					}
				}
				else
				{
					if (errno == EAGAIN || errno == EINTR)
					{
						if (!pstClientInfo->pSocketSendBuf->addData(pszData, iSize))
						{
							snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,write error buffer less than data", __FILE__, __LINE__);
							return -1;
						}

						if (m_oEvent.addFdEvent(stSession.iFd, CEvent::EV_WRITE, std::bind(&CNetWorker::onWrite, this, std::placeholders::_1, std::placeholders::_2), pstClientInfo) != 0)
						{
							snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_oEvent.getErrorMsg());
							return -1;
						}
					}
					else
					{
						snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
						return -1;
					}
				}
			}
		}
		return 0;
	}

	int CNetWorker::write(int iFd)
	{

		StClientInfo *pstClientInfo = m_vecClients[iFd];
		StServerInfo *pstServerInfo = pstClientInfo->pstServerInfo;

		StSession stSession;
		stSession.ddwBeginTime = lce::GetTickCount();
		stSession.stClientAddr = pstClientInfo->stClientAddr;
		stSession.iFd = iFd;
		stSession.iSvrId = pstServerInfo->iSrvId;

		int iSendBufSize = pstClientInfo->pSocketSendBuf->getSize();

		if (iSendBufSize > 0)
		{
			int iSendSize = lce::Send(iFd, pstClientInfo->pSocketSendBuf->getData(), iSendBufSize);
			if (iSendSize > 0)
			{
				pstClientInfo->pSocketSendBuf->removeData(iSendSize);
				iSendBufSize -= iSendSize;
			}
			else
			{
				if (errno != EAGAIN && errno != EINTR)
				{
					snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s,%d,errno:%d,error:%s", __FILE__, __LINE__, errno, strerror(errno));
					close(iFd);
					onError(stSession, m_szErrMsg, ERR_SOCKET);
					return -1;
				}
			}
		}

		if (iSendBufSize == 0)
		{
			if (pstClientInfo->bNeedClose)
				close(pstClientInfo->iFd);
			else
				m_oEvent.delFdEvent(iFd, CEvent::EV_WRITE);
		}
		return 0;
	}

	void CNetWorker::onWrite(int iFd, void *pData)
	{

		if (isClose(iFd))
		{
			return;
		}
		write(iFd);
	}

	int CNetWorker::addTimer(int iTimerId, uint32_t dwExpire, void *pData)
	{
		return m_oEvent.addTimer(iTimerId, dwExpire, std::bind(&CNetWorker::onTimer, this, std::placeholders::_1, std::placeholders::_2), pData);
	}

	int CNetWorker::delTimer(int iTimerId)
	{
		return m_oEvent.delTimer(iTimerId);
	}

	int CNetWorker::sendMessage(int iMsgType, void *pData /* = NULL */)
	{
		return m_oEvent.addMessage(iMsgType, std::bind(&CNetWorker::onMessage, this, std::placeholders::_1, std::placeholders::_2), pData);
	}

};
