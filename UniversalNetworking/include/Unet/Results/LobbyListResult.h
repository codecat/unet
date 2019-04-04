#pragma once

#include <Unet_common.h>
#include <Unet/ResultObject.h>
#include <Unet/LobbyInfo.h>

namespace Unet
{
	struct LobbyListResult : public ResultObject
	{
		std::vector<LobbyInfo> Lobbies;

		/// Adds an entry point to an existing lobby by Guid, or if the lobby doesn't exist, adds a new lobby to the list.
		LobbyInfo* AddEntryPoint(const xg::Guid &guid, const ServiceEntryPoint &newEntryPoint);
	};
}
