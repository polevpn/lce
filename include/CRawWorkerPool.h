#ifndef CPROCESSOR_POOL_H
#define CPROCESSOR_POOL_H

#include "CCommMgr.h"
#include "CProcessor.h"
#include "CRawWorker.h"
#include "CTask.h"

namespace lce
{

    typedef void (*WORKER_POOL_ERROR_HANDLER)(const char *szErrMsg);

    template <class T>
    class CRawWorkerPool : public CTask, public CProcessor
    {
    public:
        CRawWorkerPool();
        virtual ~CRawWorkerPool();
        int init(uint32_t dwThreadNum = 32, uint32_t dwQueueSize = 10000);
        void setErrHandler(WORKER_POOL_ERROR_HANDLER pErrHandler = NULL)
        {
            m_pErrHandler = pErrHandler;
        }

    public:
        void onRead(StSession &stSession, const char *pszData, const int iSize);
        void onConnect(StSession &stSession, bool bOk, void *pData) {}
        void onClose(StSession &stSession) {}
        void onError(StSession &stSession, const char *szErrMsg, int iError);
        void onMessage(int iMsgType, void *pData);

    public:
        void onWork(int iTaskType, void *pData, int iIndex);

    private:
        vector<CRawWorker *> m_vecWorkers;
        WORKER_POOL_ERROR_HANDLER m_pErrHandler;
    };

    //------------------------------------------
    template <class T>
    CRawWorkerPool<T>::~CRawWorkerPool()
    {
        for (size_t i = 0; i < m_vecWorkers.size(); i++)
        {
            delete m_vecWorkers[i];
        }
    }
    template <class T>
    CRawWorkerPool<T>::CRawWorkerPool()
    {
    }

    template <class T>
    int CRawWorkerPool<T>::init(uint32_t dwThreadNum, uint32_t dwQueueSize)
    {
        for (size_t i = 0; i < dwThreadNum; i++)
        {
            CRawWorker *poWorker = new T;
            m_vecWorkers.push_back(poWorker);
        }

        if (CTask::init(dwThreadNum, dwQueueSize) < 0)
        {
            return -1;
        }
        return 0;
    }

    template <class T>
    void CRawWorkerPool<T>::onMessage(int iMsgType, void *pData)
    {
        CRawResponse *pstResponse = (CRawResponse *)pData;
        CCommMgr::getInstance().write(pstResponse->getSession(), pstResponse->getWriter().data(), pstResponse->getWriter().size(), pstResponse->getCloseFlag());
        delete pstResponse;
        pstResponse = NULL;
    }

    template <class T>
    void CRawWorkerPool<T>::onError(StSession &stSession, const char *szErrMsg, int iError)
    {
        if (m_pErrHandler != NULL)
            m_pErrHandler(szErrMsg);
    }

    template <class T>
    void CRawWorkerPool<T>::onRead(StSession &stSession, const char *pszData, const int iSize)
    {
        CRawRequest *pstRequest = new CRawRequest;
        pstRequest->setSession(stSession);
        pstRequest->getReader().assign(pszData, iSize);
        if (dispatch(0, pstRequest) < 0)
        {
            if (m_pErrHandler != NULL)
                m_pErrHandler("task queue full");
        }
    }

    template <class T>
    void CRawWorkerPool<T>::onWork(int iTaskType, void *pData, int iIndex)
    {
        CRawRequest *pstRequest = (CRawRequest *)pData;
        CRawResponse *pstResponse = new CRawResponse;
        pstResponse->setSession(pstRequest->getSession());
        try
        {
            m_vecWorkers[iIndex]->onRequest(*pstRequest, *pstResponse);
            if (CCommMgr::getInstance().sendMessage(iTaskType, this, pstResponse) < 0)
            {
                if (m_pErrHandler != NULL)
                    m_pErrHandler(CCommMgr::getInstance().getErrMsg());
            }
        }
        catch (const exception &e)
        {
            if (m_pErrHandler != NULL)
                m_pErrHandler(e.what());

            delete pstResponse;
            pstResponse = NULL;
        }
        delete pstRequest;
        pstRequest = NULL;
    }

};

#endif // CPROCESSOR_POOL_H
