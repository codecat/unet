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

	struct LobbyMember
	{
		xg::Guid UnetGuid;
		int UnetPeer = -1;

		std::string Name;
		std::vector<ServiceID> IDs;
		std::vector<LobbyData> Data;

		ServiceID* GetServiceID(ServiceType type);
	};

	class Lobby
	{
		friend class Context;

	private:
		Context* m_ctx;
		LobbyInfo m_info;

		std::vector<LobbyData> m_data;
		std::vector<LobbyMember> m_members;

	public:
		Lobby(Context* ctx, const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();
		const std::vector<LobbyMember> &GetMembers();

		void AddEntryPoint(ServiceID id);
		LobbyMember &AddMemberService(const xg::Guid &guid, const ServiceID &id);
		void ServiceDisconnected(ServiceType service);

		void SetData(const char* name, const std::string &value);
		std::string GetData(const char* name);
		const std::vector<LobbyData> &GetData();
	};
}
