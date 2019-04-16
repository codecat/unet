#pragma once

#include <Unet_common.h>
#include <Unet/LobbyInfo.h>
#include <Unet/LobbyMember.h>
#include <Unet/LobbyData.h>

namespace Unet
{
	class Lobby : public LobbyDataContainer
	{
		friend class Context;

	private:
		Context* m_ctx;
		LobbyInfo m_info;

		std::vector<LobbyMember> m_members;

	public:
		Lobby(Context* ctx, const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();

		ServiceID GetPrimaryEntryPoint();

		const std::vector<LobbyMember> &GetMembers();
		LobbyMember* GetMember(const xg::Guid &guid);
		LobbyMember* GetMember(int peer);
		LobbyMember* GetMember(const ServiceID &serviceId);
		LobbyMember* GetHostMember();

		void AddEntryPoint(const ServiceID &id);
		void ServiceDisconnected(ServiceType service);

		LobbyMember &AddMemberService(const xg::Guid &guid, const ServiceID &id);
		void RemoveMemberService(const ServiceID &id);
		void RemoveMember(const LobbyMember &member);

		virtual void SetData(const std::string &name, const std::string &value) override;
		virtual std::string GetData(const std::string &name) const override;

	private:
		int GetNextAvailablePeer();
	};
}
