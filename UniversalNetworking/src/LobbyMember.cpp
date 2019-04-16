#include <Unet_common.h>
#include <Unet/LobbyMember.h>
#include <Unet/Context.h>

Unet::LobbyMember::LobbyMember(Context* ctx)
{
	m_ctx = ctx;
}

Unet::ServiceID Unet::LobbyMember::GetServiceID(ServiceType type) const
{
	for (auto &id : IDs) {
		if (id.Service == type) {
			return id;
		}
	}
	return ServiceID();
}

Unet::ServiceID Unet::LobbyMember::GetDataServiceID() const
{
	assert(IDs.size() > 0);

	// Prefer our primary service, if the client supports it
	for (auto &id : IDs) {
		if (id.Service == m_ctx->m_primaryService) {
			return id;
		}
	}

	// Prefer the client's primary service, if we support it
	for (auto &id : IDs) {
		if (id.Service == UnetPrimaryService) {
			if (m_ctx->GetService(id.Service) != nullptr) {
				return id;
			}
			break;
		}
	}

	// As a fallback, just pick any ID that we support
	for (auto &id : IDs) {
		if (m_ctx->GetService(id.Service) != nullptr) {
			return id;
		}
	}

	// We can't send messages to this member
	return ServiceID();
}

Unet::ServiceID Unet::LobbyMember::GetPrimaryServiceID() const
{
	return GetServiceID(UnetPrimaryService);
}
