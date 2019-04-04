#pragma once

#include <Unet_common.h>
#include <Unet/LobbyInfo.h>

namespace Unet
{
	struct LobbyData
	{
		std::string Name;
		std::string Value;
	};

	class Lobby
	{
	private:
		Context* m_ctx;
		LobbyInfo m_info;

		std::vector<LobbyData> m_data;

	public:
		Lobby(Context* ctx, const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();

		void AddEntryPoint(ServiceEntryPoint entryPoint);
		void ServiceDisconnected(ServiceType service);

		void SetData(const char* name, const std::string &value);
		std::string GetData(const char* name);
		const std::vector<LobbyData> &GetData();
	};
}
