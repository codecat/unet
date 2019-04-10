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

	class LobbyMember
	{
	private:
		Context* m_ctx;

	public:
		/// Only true if all fundamental user data has been received after joining the lobby (should only be a concern on the host)
		bool Valid = true;

		xg::Guid UnetGuid;
		int UnetPeer = -1;

		/// The primary service this member uses to communicate (this is decided by which service the Hello packet is sent through)
		ServiceType UnetPrimaryService = ServiceType::None;

		std::string Name;
		std::vector<ServiceID> IDs;
		std::vector<LobbyData> Data;

	public:
		LobbyMember(Context* ctx);

		ServiceID GetServiceID(ServiceType type) const;
		ServiceID GetDataServiceID() const;
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

		void SetData(const char* name, const std::string &value);
		std::string GetData(const char* name);
		const std::vector<LobbyData> &GetData();
	};
}
