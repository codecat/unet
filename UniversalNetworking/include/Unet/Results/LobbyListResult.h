#pragma once

#include <Unet_common.h>
#include <Unet/ResultObject.h>
#include <Unet/LobbyInfo.h>

namespace Unet
{
	struct LobbyListResult : public ResultObject
	{
		std::vector<LobbyInfo> Lobbies;
	};
}
