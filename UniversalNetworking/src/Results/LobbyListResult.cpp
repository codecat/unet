#include <Unet_common.h>
#include <Unet/Results/LobbyListResult.h>

Unet::LobbyInfo* Unet::LobbyListResult::AddEntryPoint(const xg::Guid &guid, const ServiceEntryPoint &newEntryPoint)
{
	for (auto &lobbyInfo : Lobbies) {
		if (lobbyInfo.UnetGuid != guid) {
			continue;
		}

		if (lobbyInfo.GetEntryPoint(newEntryPoint.Service) != nullptr) {
			assert(false); // Entry point already exists in the lobby info!
			return nullptr;
		}

		lobbyInfo.EntryPoints.emplace_back(newEntryPoint);
		return &lobbyInfo;
	}

	LobbyInfo newLobbyInfo;
	newLobbyInfo.UnetGuid = guid;
	newLobbyInfo.EntryPoints.emplace_back(newEntryPoint);
	Lobbies.emplace_back(newLobbyInfo);

	return &Lobbies[Lobbies.size() - 1];
}
