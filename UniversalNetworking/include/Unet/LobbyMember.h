#pragma once

#include <Unet_common.h>
#include <Unet/ServiceID.h>
#include <Unet/LobbyData.h>

namespace Unet
{
	class LobbyMember : public LobbyDataContainer
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

	public:
		LobbyMember(Context* ctx);

		ServiceID GetServiceID(ServiceType type) const;
		ServiceID GetDataServiceID() const;
		ServiceID GetPrimaryServiceID() const;
	};
}
