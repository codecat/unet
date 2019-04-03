#pragma once

#include <Unet_common.h>
#include <Unet/LobbyInfo.h>
#include <Unet/ServiceType.h>

namespace Unet
{
	class Service
	{
	public:
		virtual ServiceType GetType() = 0;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) = 0;
		virtual void GetLobbyList() = 0;
		virtual void LeaveLobby() = 0;
	};
}
