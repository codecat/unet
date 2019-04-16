#pragma once

#include <Unet_common.h>
#include <Unet/Results/CreateLobbyResult.h>
#include <Unet/Results/LobbyListResult.h>
#include <Unet/Results/LobbyJoinResult.h>
#include <Unet/Results/LobbyLeftResult.h>

namespace Unet
{
	class Callbacks
	{
	public:
		// Generic logging functions
		virtual void OnLogError(const std::string &str) {}
		virtual void OnLogWarn(const std::string &str) {}
		virtual void OnLogInfo(const std::string &str) {}
		virtual void OnLogDebug(const std::string &str) {}

		// Callbacks for results
		virtual void OnLobbyCreated(const CreateLobbyResult &result) {}
		virtual void OnLobbyList(const LobbyListResult &result) {}
		virtual void OnLobbyJoined(const LobbyJoinResult &result) {}
		virtual void OnLobbyLeft(const LobbyLeftResult &result) {}

		// Lobby member events
		virtual void OnLobbyPlayerJoined(const LobbyMember &member) {}
		virtual void OnLobbyPlayerLeft(const LobbyMember &member) {}

		// Lobby data events
		virtual void OnLobbyDataChanged(const std::string &name) {}
		virtual void OnLobbyMemberDataChanged(LobbyMember &member, const std::string &name) {}
	};
}
