#pragma once

#include <Unet_common.h>
#include <Unet/ServiceEntryPoint.h>

namespace Unet
{
	enum class LobbyPrivacy
	{
		Public,
		Private,
	};

	struct LobbyInfo
	{
		//TODO: Some unique ID so that it can be identified across services

		bool IsHosting = false;
		LobbyPrivacy Privacy = LobbyPrivacy::Public;
		int MaxPlayers = 0;

		std::string Name;
		std::vector<ServiceEntryPoint> EntryPoints;

		const ServiceEntryPoint* GetEntryPoint(ServiceType service) const;
	};
}
