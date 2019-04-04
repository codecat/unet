#pragma once

#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/ServiceType.h>

namespace Unet
{
	class Service
	{
	public:
		Context* m_ctx = nullptr;

	public:
		virtual ServiceType GetType() = 0;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) = 0;
		virtual void GetLobbyList() = 0;
		virtual void JoinLobby(uint64_t lobbyId) = 0;
		virtual void LeaveLobby() = 0;

		virtual int GetLobbyMaxPlayers(uint64_t lobbyId) = 0;

		virtual std::string GetLobbyData(uint64_t lobbyId, const char* name) = 0;
		virtual int GetLobbyDataCount(uint64_t lobbyId) = 0;
		virtual LobbyData GetLobbyData(uint64_t lobbyId, int index) = 0;

		virtual void SetLobbyData(uint64_t lobbyId, const char* name, const char* value) = 0;
	};
}
