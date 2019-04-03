#pragma once

#include <Unet_common.h>
#include <Unet/LobbyInfo.h>

namespace Unet
{
	class Lobby
	{
	private:
		Context* m_ctx;
		LobbyInfo m_info;

	public:
		Lobby(Context* ctx, const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();

		void AddEntryPoint(ServiceEntryPoint entryPoint);
		void ServiceDisconnected(ServiceType service);
	};
}
