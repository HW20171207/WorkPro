/************C++ Source File****************
#
#	Filename: IOCP.cpp
#
#	Author: H`W
#	Description: ---
#	Create: 2018-08-11 17:40:47
#	Last Modified: 2018-08-11 17:40:47
*******************************************/



#include "IOCP.h"
#include "Event.h"

#if defined _WIN32 && defined IOCP_ENABLE

namespace chaos
{
	IOCP::AcceptExPtr IOCP::s_acceptEx = (AcceptExPtr)IOCP::GetExtensionFunction(WSAID_ACCEPTEX);
	IOCP::ConnectExPtr IOCP::s_connectEx = (ConnectExPtr)IOCP::GetExtensionFunction(WSAID_CONNECTEX);
	IOCP::GetAcceptExSockaddrsPtr IOCP::s_getAcceptExSockaddrs = (GetAcceptExSockaddrsPtr)IOCP::GetExtensionFunction(WSAID_GETACCEPTEXSOCKADDRS);


	IOCP::IOCP(EventCentre* pCentre):
		Poller(pCentre),
		m_completionPort(0),
		m_isInit(false),
		m_workThreads(0),
		m_threadHandles(NULL),
		//m_liveThreads(0),
		m_tids(NULL)
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		m_workThreads = systemInfo.dwNumberOfProcessors;

		m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_workThreads);
		m_threadHandles = new HANDLE[m_workThreads]{ 0 };
		m_tids = new thread_t[m_workThreads]{ 0 };
	}


	IOCP::~IOCP()
	{
		for (DWORD i = 0; i < m_workThreads; ++i)
		{
			PostQueuedCompletionStatus(m_completionPort, 0, NOTIFY_SHUTDOWN_KEY, NULL);
		}

		//if (0 != m_liveThreads)
		//	m_sem.SemWait();

		if (m_threadHandles)
		{
			for (DWORD i = 0; i < m_workThreads; ++i)
			{
				int ret = WaitForSingleObject(m_threadHandles[i], INFINITE);
				if (ret != 0)
				{
					//通常不会执行这里
					printf("wait thread end failed! force thread.%d\n", GetLastError());
					TerminateThread(m_threadHandles[i], -1);
				}

				CloseHandle(m_threadHandles[i]);
			}
			delete[] m_threadHandles;
		}

		if (m_tids)
			delete[] m_tids;

		if (m_completionPort)
			CloseHandle(m_completionPort);
	}


	int IOCP::Init()
	{
		if (m_isInit)
			return 0;

		for (DWORD i = 0; i < m_workThreads; ++i)
		{
			m_threadHandles[i] = (HANDLE)_beginthreadex(NULL, 0, &IOCP::Loop, this, 0, &m_tids[i]);
			if (!m_threadHandles)
				return -1;
		}

		//if (!s_acceptEx || !s_connectEx || !s_getAcceptExSockaddrs)
		//{
		//	//获取Ex系列函数
		//	GUID acceptex = WSAID_ACCEPTEX;
		//	GUID connectex = WSAID_CONNECTEX;
		//	GUID getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
		//	Socket s(AF_INET, SOCK_STREAM, 0);

		//	if (INVALID_SOCKET == s.GetFd())
		//		return -1;

		//	socket_t fd = s.GetFd();
		//	DWORD bytes = 0;

		//	if (0 != WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex, sizeof(acceptex),
		//		&s_acceptEx, sizeof(s_acceptEx), &bytes, NULL, NULL))
		//		return -1;

		//	if (0 != WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectex, sizeof(connectex),
		//		&s_connectEx, sizeof(s_connectEx), &bytes, NULL, NULL))
		//		return -1;

		//	if (0 != WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &getacceptexsockaddrs, sizeof(getacceptexsockaddrs),
		//		&s_getAcceptExSockaddrs, sizeof(s_getAcceptExSockaddrs), &bytes, NULL, NULL))
		//		return -1;
		//}

		m_isInit = true;

		return 0;
	}


	BOOL IOCP::AcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
		DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
	{
		if (!s_acceptEx)
			return false;
		
		return s_acceptEx(sListenSocket, sAcceptSocket, lpOutputBuffer, dwReceiveDataLength,
			dwLocalAddressLength, dwRemoteAddressLength, lpdwBytesReceived, lpOverlapped);
	}


	BOOL IOCP::ConnectEx(SOCKET s, const struct sockaddr* name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength,
		LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped)
	{
		if (!s_connectEx)
			return false;
		
		return s_connectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
	}


	void IOCP::GetAcceptExSockeaddrs(PVOID lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength,
		DWORD dwRemoteAddressLength, LPSOCKADDR* LocalSockaddr, LPINT LocalSockaddrLength, LPSOCKADDR * RemoteSockaddr, LPINT RemoteSockaddrLength)
	{
		if (!s_getAcceptExSockaddrs)
			return;

		s_getAcceptExSockaddrs(lpOutputBuffer, dwReceiveDataLength, dwLocalAddressLength, dwRemoteAddressLength,
			LocalSockaddr, LocalSockaddrLength, RemoteSockaddr, RemoteSockaddrLength);
	}


	int IOCP::RegistFd(socket_t fd, short ev)
	{
		HANDLE ret = CreateIoCompletionPort((HANDLE)fd, m_completionPort, NULL/*(DWORD)pKeyData*/, 0);
		if (!ret)
			return -1;

		return 0;
	}


	void* IOCP::GetExtensionFunction(const GUID& funcGUID)
	{
		WsaData::Instance();

		Socket s(AF_INET, SOCK_STREAM, 0);

		if (s.GetFd() == INVALID_SOCKET)
		{
			printf("create socket failed:%d\n", WSAGetLastError());
			return NULL;
		}

		void* pFunc = NULL;
		DWORD bytes = 0;

		WSAIoctl(s.GetFd(), SIO_GET_EXTENSION_FUNCTION_POINTER, &(const_cast<GUID&>(funcGUID)),
			sizeof(funcGUID), &pFunc, sizeof(pFunc), &bytes, NULL, NULL);

		return pFunc;
	}


	int IOCP::Launch(int timeoutMs, EventList& activeEvents)
	{
		if (!activeEvents.empty())
			return 0;

		if (0 != timeoutMs)
		{
			m_pCentre->WaitWaittintEvsCond(timeoutMs);
		}

		return 0;
	}


	unsigned int IOCP::Loop(void* arg)
	{
		assert(arg);

		IOCP* iocp = (IOCP*)arg;
		EventCentre& centre = iocp->GetCentre();

		//iocp->AddLiveThread();

		while (1)
		{
			DWORD bytes = 0;
			ULONG_PTR key = 0;
			OVERLAPPED *overlapped = NULL;
			bool bOk = GetQueuedCompletionStatus(iocp->m_completionPort, &bytes, &key, &overlapped, WSA_INFINITE);

			//结束GetQueuedCompletionStatus 准备退出工作线程
			if (NOTIFY_SHUTDOWN_KEY == key)
				break;

			if (overlapped)
			{
				LPCOMPLETION_OVERLAPPED lo = (LPCOMPLETION_OVERLAPPED)overlapped;

				//这里lo一定不会被其他线程释放掉(一定是一个有效的内存)
				//因为释放lo前已经确保lo未被投递到WSA请求中
				if (lo->cb)
				{
					MutexGuard lock(centre.GetMutex());													
					lo->cb(overlapped, bytes, key, bOk);
				}
			}

			else
			{
				printf("GetQueuedCompletionStatus return overlapped is null!\n");
			}
		}

		//if (iocp->DecLiveThread() == 0)
		//	iocp->m_sem.SemPost();

		return 0;
	}

}

#endif // !_WIN32