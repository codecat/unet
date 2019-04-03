#pragma once

#include <Unet_common.h>
#include <Unet/LobbyInfo.h>

namespace Unet
{
	class Lobby
	{
	public:
		Lobby(const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();

		void AddEntryPoint(ServiceEntryPoint entryPoint);
		void ServiceDisconnected(ServiceType service);

	private:
		LobbyInfo m_info;
	};
}
