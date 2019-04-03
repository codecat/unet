#pragma once

#include <Unet_common.h>
#include <Unet/ServiceEntryPoint.h>
#include <guid.hpp>

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

		xg::Guid UnetGuid;

		std::string Name;
		std::vector<ServiceEntryPoint> EntryPoints;

		const ServiceEntryPoint* GetEntryPoint(ServiceType service) const;
	};
}
