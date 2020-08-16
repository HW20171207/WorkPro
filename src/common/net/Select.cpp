/************C++ Source File****************
#
#	Filename: Select.cpp
#
#	Author: H`W
#	Description: ---
#	Create: 2018-08-11 17:40:25
#	Last Modified: 2018-08-11 17:40:25
*******************************************/


#include "Select.h"
#include "Event.h"

namespace chaos
{

	//Select& Select::Instance()
	//{
	//	static Select s_inst;
	//	return s_inst;
	//}


	Select::Select(EventCentre* pCentre):
		Poller(pCentre)
	{
		FD_ZERO(&m_rfds);
		FD_ZERO(&m_wfds);
		FD_ZERO(&m_efds);

	}


	Select::~Select()
	{

	}


	int Select::Init()
	{
		return 0;
	}


	int Select::Launch(int timeoutMs)
	{
		if (0 >= timeoutMs)
			timeoutMs = NET_TICK;

		fd_set rfds = m_rfds;
		fd_set wfds = m_wfds;
		fd_set efds = m_efds;

#ifdef _WIN32
		//windows中的select调用不允许传入空的fd_set
		if (0 >= (rfds.fd_count + wfds.fd_count + efds.fd_count))
		{
			Sleep(timeoutMs);
			return 0;
		}
#endif // _WIN32

		timeval val{0, timeoutMs * 1000};

		int cnt = select(MAX_FD, &rfds, &wfds, &efds, &val);

		if (0 > cnt)
		{
			printf("call select failed! code:%d\n", WSAGetLastError());
			return cnt;
		}


		else if(0 < cnt)
		{
			printf("select cnt:%d\n", cnt);
		}

		if (0 == cnt)
			return 0;//continue;

		CollectEvent(rfds, wfds, efds);

		return 0;
	}


	void Select::CollectEvent(const fd_set& rfds, const fd_set& wfds, const fd_set& efds)
	{
#ifdef _WIN32

		for (uint32 i = 0; i < rfds.fd_count; ++i)
		{
			PushActiveEvent(rfds.fd_array[i], EV_IOREAD);
		}

		for (uint32 i = 0; i < wfds.fd_count; ++i)
		{
			PushActiveEvent(wfds.fd_array[i], EV_IOWRITE);
		}

		for (uint32 i = 0; i < efds.fd_count; ++i)
		{
			PushActiveEvent(efds.fd_array[i], EV_IOEXCEPT);
		}

#else
		auto fds = GetAllEvents();
		for (auto it = fds.begin(); it != fds.end(); ++it)
		{
			socket_t fd = it->first;
			short ev = 0;

			if (FD_ISSET(fd, &rfds))
				ev |= EV_IOREAD;

			if (FD_ISSET(fd, &wfds))
				ev |= EV_IOWRITE;

			if (FD_ISSET(fd, &efds))
				ev |= EV_IOEXCEPT;

			else
				continue;

			PushActiveEvent(fd, ev);
		}


#endif // _WIN32

	}


	int Select::RegistFd(const Event* pEvent)
	{
		if (!pEvent)
			return -1;

		const EventKey& key = pEvent->GetEvKey();
		socket_t fd = key.fd;
		short ev = pEvent->GetEv();

		if (ev & EV_IOREAD)
			FD_SET(fd, &m_rfds);
		if (ev & EV_IOWRITE)
			FD_SET(fd, &m_wfds);
		if (ev & EV_IOEXCEPT)
			FD_SET(fd, &m_efds);

		return 0;
	}


	int Select::CancelFd(socket_t fd)
	{
		FD_CLR(fd, &m_rfds);
		FD_CLR(fd, &m_wfds);
		FD_CLR(fd, &m_efds);

		return 0;
	}

}