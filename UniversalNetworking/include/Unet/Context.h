#pragma once

#include <Unet_common.h>
#include <Unet/Callbacks.h>
#include <Unet/ServiceType.h>
#include <Unet/Lobby.h>
#include <Unet/Service.h>
#include <Unet/MultiCallback.h>
#include <Unet/NetworkMessage.h>

namespace Unet
{
	enum class ContextStatus
	{
		Idle,
		Connecting,
		Connected,
	};

	class Context
	{
		friend class Lobby;
		friend class LobbyMember;

	public:
		Context();
		virtual ~Context();

		ContextStatus GetStatus();

		/// Context takes ownership of the callbacks object
		void SetCallbacks(Callbacks* callbacks);
		Callbacks* GetCallbacks();
		void RunCallbacks();

		void SetPrimaryService(ServiceType service);
		void EnableService(ServiceType service);
		int ServiceCount();

		void CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name = nullptr);
		void GetLobbyList();
		void JoinLobby(LobbyInfo &lobbyInfo);
		void JoinLobby(const ServiceID &id);
		void LeaveLobby();

		/// Get maximum number of players as reported by services.
		int GetLobbyMaxPlayers(const LobbyInfo &lobbyInfo);

		/// Get lobby data as reported by services.
		std::string GetLobbyData(const LobbyInfo &lobbyInfo, const char* name);
		/// Get all lobby data as reported by services.
		std::vector<LobbyData> GetLobbyData(const LobbyInfo &lobbyInfo);

		Lobby* CurrentLobby();
		int GetLocalPeer();

		void SetPersonaName(const std::string &str);
		const std::string &GetPersonaName();

		bool IsMessageAvailable(int channel);
		std::unique_ptr<NetworkMessage> ReadMessage(int channel);

		void SendTo(LobbyMember &member, uint8_t* data, size_t size, PacketType type = PacketType::Reliable, int channel = 0);
		void SendToAll(uint8_t* data, size_t size, PacketType type = PacketType::Reliable, int channel = 0);
		void SendToAllExcept(LobbyMember &exceptMember, uint8_t* data, size_t size, PacketType type = PacketType::Reliable, int channel = 0);
		void SendToHost(uint8_t* data, size_t size, PacketType type = PacketType::Reliable, int channel = 0);

	private:
		Service* PrimaryService();
		Service* GetService(ServiceType type);

		void InternalSendTo(LobbyMember &member, uint8_t* data, size_t size);
		void InternalSendToAll(uint8_t* data, size_t size);
		void InternalSendToAllExcept(LobbyMember &exceptMember, uint8_t* data, size_t size);
		void InternalSendToHost(uint8_t* data, size_t size);

	private:
		void OnLobbyCreated(const CreateLobbyResult &result);
		void OnLobbyList(const LobbyListResult &result);
		void OnLobbyJoined(const LobbyJoinResult &result);
		void OnLobbyLeft(const LobbyLeftResult &result);

		void OnLobbyPlayerLeft(const LobbyMember &member);

	private:
		std::string m_personaName;

		ContextStatus m_status;
		ServiceType m_primaryService;

		Callbacks* m_callbacks;

		Lobby* m_currentLobby;
		xg::Guid m_localGuid;
		int m_localPeer;

		std::vector<Service*> m_services;

	public:
		MultiCallback<CreateLobbyResult> m_callbackCreateLobby;
		MultiCallback<LobbyListResult> m_callbackLobbyList;
		MultiCallback<LobbyJoinResult> m_callbackLobbyJoin;
		MultiCallback<LobbyLeftResult> m_callbackLobbyLeft;
	};
}
