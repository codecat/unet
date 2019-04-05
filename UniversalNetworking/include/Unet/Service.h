#pragma once

#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/ServiceType.h>

namespace Unet
{
	enum class PacketType
	{
		Unreliable,
		Reliable,
	};

	class Service
	{
	public:
		Context* m_ctx = nullptr;

	public:
		virtual ServiceType GetType() = 0;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) = 0;
		virtual void GetLobbyList() = 0;
		virtual void JoinLobby(const ServiceID &id) = 0;
		virtual void LeaveLobby() = 0;

		virtual int GetLobbyMaxPlayers(const ServiceID &lobbyId) = 0;

		virtual std::string GetLobbyData(const ServiceID &lobbyId, const char* name) = 0;
		virtual int GetLobbyDataCount(const ServiceID &lobbyId) = 0;
		virtual LobbyData GetLobbyData(const ServiceID &lobbyId, int index) = 0;

		virtual void SetLobbyData(const ServiceID &lobbyId, const char* name, const char* value) = 0;

		virtual void SendPacket(const ServiceID &peerId, const void* data, size_t size, PacketType type, uint8_t channel) = 0;
		virtual size_t ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel) = 0;
		virtual bool IsPacketAvailable(size_t* outPacketSize, uint8_t channel) = 0;
	};
}
