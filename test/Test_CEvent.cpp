#include <iostream>
#include "Utils.h"
#include "CEvent.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>

using namespace std;
using namespace lce;

const int MAXLINE = 4096;

CEvent oCEvent;

typedef struct
{
    char szBuf[MAXLINE + 1];
    int iSize;
} SESession;

void onWrite(int iFd, void *pData)
{
    cout << "onWrite " << iFd << endl;
    SESession *pSession = (SESession *)pData;
    ::send(iFd, pSession->szBuf, pSession->iSize, 0);
    oCEvent.delFdEvent(iFd, CEvent::EV_WRITE);
    //::close(iFd);
    delete pSession;
}

void onRead(int iFd, void *pData)
{
    cout << "onRead " << iFd << endl;

    SESession *pSession = new SESession();

    pSession->iSize = ::read(iFd, pSession->szBuf, MAXLINE);

    if (pSession->iSize > 0)
    {
        pSession->szBuf[pSession->iSize] = '\0';
        cout << pSession->szBuf << endl;
        oCEvent.addFdEvent(iFd, CEvent::EV_WRITE, onWrite, pSession);
    }
    else if (pSession->iSize == 0)
    {
        delete pSession;
        oCEvent.delFdEvent(iFd, CEvent::EV_WRITE | CEvent::EV_READ);
        lce::Close(iFd);
        cout << "onClose " << iFd << endl;
    }
    else
    {
        delete pSession;
        oCEvent.delFdEvent(iFd, CEvent::EV_WRITE | CEvent::EV_READ);
        lce::Close(iFd);
    }
}

void onAccept(int iFd, void *pData)
{
    cout << "onAccept" << endl;

    struct sockaddr_in stClientAddr;
    int iAddrLen = sizeof(struct sockaddr_in);

    int iClientSock = accept(iFd, (struct sockaddr *)&stClientAddr, (socklen_t *)&iAddrLen);
    if (iClientSock < 0)
    {
        cout << "accept error" << endl;
        return;
    }

    cout << "client fd:" << iClientSock << endl;
    lce::SetNoneBlock(iClientSock);
    oCEvent.addFdEvent(iClientSock, CEvent::EV_READ, onRead, NULL);
}

void onConnect(int iFd, void *pData)
{

    oCEvent.delFdEvent(iFd, CEvent::EV_READ | CEvent::EV_WRITE);
    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(iFd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0)
    {
        cout << "fail1" << endl;
        return;
    }
    if (err == 0)
    {
        cout << "onConnect" << endl;
    }
    else
    {
        cout << "onConnectFail" << endl;
    }

    oCEvent.addFdEvent(iFd, CEvent::EV_READ, onRead, NULL);
    cout << "onConnect" << endl;
}

void onMessage(uint32_t dwMsgType, void *pData)
{
    cout << "onMessage" << endl;
}

void onTimer(uint32_t dwTimerId, void *pData)
{

    cout << "onTimer" << endl;
    oCEvent.addMessage(1, onMessage, NULL);
    oCEvent.addMessage(1, onMessage, NULL);
    oCEvent.addTimer(dwTimerId, 2000, onTimer, NULL);
    /*
        int iFd=lce::CreateTcpSock();

        cout<<"fd="<<iFd<<endl;
        lce::SetNoneBlock(iFd);

        int iRet=lce::Connect(iFd,"127.0.0.1",80);

        if(iRet != -1)
        {
            cout<<"connect"<<endl;
        }
        else
        {
            if (errno == EINPROGRESS)
            {
                cout<<"add event"<<endl;
                oCEvent.addFdEvent(iFd,CEvent::EV_WRITE,onConnect,NULL);
            }
        }
    */
}

int main()
{
    int iSrvSock = 0;
    iSrvSock = lce::CreateTcpSock();
    if (iSrvSock < 0)
    {
        cout << "onConnect" << endl;
        printf("error=%d,msg=%s", errno, strerror(errno));
        return 0;
    }

    lce::SetReUseAddr(iSrvSock);
    lce::SetNoneBlock(iSrvSock);

    if (lce::Bind(iSrvSock, "127.0.0.1", 3000) < 0)
    {
        printf("bind error=%d,msg=%s", errno, strerror(errno));
        return 0;
    }
    lce::Listen(iSrvSock);

    int iRet = oCEvent.init();
    if (iRet < 0)
    {
        printf("msg=%s", oCEvent.getErrorMsg());
        return 0;
    }

    oCEvent.addTimer(0, 2000, onTimer, NULL);
    if (oCEvent.addFdEvent(iSrvSock, CEvent::EV_READ, onAccept, NULL) != 0)
    {
        printf("xxxxxxxxxxxxx1");
    }
    oCEvent.run();
    return 0;
}
