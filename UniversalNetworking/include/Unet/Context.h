#pragma once

#include <Unet_common.h>
#include <Unet/Callbacks.h>
#include <Unet/ServiceType.h>
#include <Unet/Lobby.h>
#include <Unet/Service.h>
#include <Unet/MultiCallback.h>
#include <Unet/NetworkMessage.h>
#include <Unet/Reassembly.h>

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
		friend struct LobbyListResult;

	public:
		Context(int numChannels = 1);
		virtual ~Context();

		ContextStatus GetStatus();

		/// Context takes ownership of the callbacks object
		void SetCallbacks(Callbacks* callbacks);
		Callbacks* GetCallbacks();
		void RunCallbacks();
		void HandleLobbyMessage(ServiceID peer, uint8_t* data, size_t size);

		void SetPrimaryService(ServiceType service);
		void EnableService(ServiceType service);
		int ServiceCount();
		void SimulateServiceOutage(ServiceType service);

		void CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name = nullptr);
		void GetLobbyList();
		void JoinLobby(LobbyInfo &lobbyInfo);
		void JoinLobby(const ServiceID &id);
		void LeaveLobby(LeaveReason reason = LeaveReason::UserLeave);

		Lobby* CurrentLobby();
		int GetLocalPeer();

		void SetPersonaName(const std::string &str);
		const std::string &GetPersonaName();

		bool IsMessageAvailable(int channel);
		std::unique_ptr<NetworkMessage> ReadMessage(int channel);

		void SendTo_Impl(LobbyMember &member, uint8_t* data, size_t size, PacketType type = PacketType::Reliable, uint8_t channel = 0);
		void SendTo(LobbyMember &member, uint8_t* data, size_t size, PacketType type = PacketType::Reliable, uint8_t channel = 0);
		void SendToAll(uint8_t* data, size_t size, PacketType type = PacketType::Reliable, uint8_t channel = 0);
		void SendToAllExcept(LobbyMember &exceptMember, uint8_t* data, size_t size, PacketType type = PacketType::Reliable, uint8_t channel = 0);
		void SendToHost(uint8_t* data, size_t size, PacketType type = PacketType::Reliable, uint8_t channel = 0);

		void Kick(LobbyMember &member);

	private:
		Service* PrimaryService();
		Service* GetService(ServiceType type);

		void InternalSendTo(LobbyMember &member, const json &js);
		void InternalSendToAll(const json &js);
		void InternalSendToAllExcept(LobbyMember &exceptMember, const json &js);
		void InternalSendToHost(const json &js);

	private:
		void OnLobbyCreated(const CreateLobbyResult &result);
		void OnLobbyList(const LobbyListResult &result);
		void OnLobbyJoined(const LobbyJoinResult &result);
		void OnLobbyLeft(const LobbyLeftResult &result);

		void OnLobbyPlayerLeft(const LobbyMember &member);

	private:
		std::string m_personaName;

		int m_numChannels;

		ContextStatus m_status;
		ServiceType m_primaryService;

		Callbacks* m_callbacks;

		Lobby* m_currentLobby;
		xg::Guid m_localGuid;
		int m_localPeer;

		std::vector<Service*> m_services;

		std::vector<std::queue<NetworkMessage*>> m_queuedMessages;
		Reassembly m_reassembly;

	public:
		MultiCallback<CreateLobbyResult> m_callbackCreateLobby;
		MultiCallback<LobbyListResult> m_callbackLobbyList;
		MultiCallback<LobbyJoinResult> m_callbackLobbyJoin;
		MultiCallback<LobbyLeftResult> m_callbackLobbyLeft;
	};
}
