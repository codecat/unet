#pragma once

#include <Unet_common.h>
#include <Unet/Callbacks.h>
#include <Unet/ServiceType.h>
#include <Unet/Lobby.h>
#include <Unet/Service.h>
#include <Unet/MultiCallback.h>

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

		void CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name = nullptr);
		void GetLobbyList();
		void JoinLobby(LobbyInfo &lobbyInfo);
		void LeaveLobby();

		/// Get maximum number of players as reported by services.
		int GetLobbyMaxPlayers(const LobbyInfo &lobbyInfo);
		/// Get lobby data as reported by services.
		std::string GetLobbyData(const LobbyInfo &lobbyInfo, const char* name);
		/// Get all lobby data as reported by services.
		std::vector<LobbyData> GetLobbyData(const LobbyInfo &lobbyInfo);

		Lobby* CurrentLobby();

	private:
		Service* PrimaryService();
		Service* GetService(ServiceType type);

	private:
		void OnLobbyCreated(const CreateLobbyResult &result);
		void OnLobbyList(const LobbyListResult &result);
		void OnLobbyJoined(const LobbyJoinResult &result);
		void OnLobbyLeft(const LobbyLeftResult &result);

	private:
		ContextStatus m_status;
		ServiceType m_primaryService;

		Callbacks* m_callbacks;

		Lobby* m_currentLobby;

		std::vector<Service*> m_services;

	public:
		MultiCallback<CreateLobbyResult> m_callbackCreateLobby;
		MultiCallback<LobbyListResult> m_callbackLobbyList;
		MultiCallback<LobbyJoinResult> m_callbackLobbyJoin;
		MultiCallback<LobbyLeftResult> m_callbackLobbyLeft;
	};
}
