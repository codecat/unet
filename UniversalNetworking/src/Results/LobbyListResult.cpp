#include <Unet_common.h>
#include <Unet/Results/LobbyListResult.h>

void Unet::LobbyListResult::AddEntryPoint(const xg::Guid &guid, const ServiceEntryPoint &newEntryPoint)
{
	for (auto &lobbyInfo : Lobbies) {
		if (lobbyInfo.UnetGuid != guid) {
			continue;
		}

		if (lobbyInfo.GetEntryPoint(newEntryPoint.Service) != nullptr) {
			assert(false); // Entry point already exists in the lobby info!
			return;
		}

		lobbyInfo.EntryPoints.emplace_back(newEntryPoint);
		return;
	}

	LobbyInfo newLobbyInfo;
	newLobbyInfo.UnetGuid = guid;
	newLobbyInfo.EntryPoints.emplace_back(newEntryPoint);
	Lobbies.emplace_back(newLobbyInfo);
}
