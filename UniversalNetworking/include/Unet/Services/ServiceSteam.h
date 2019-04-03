#pragma once

#include <Unet_common.h>
#include <Unet/Service.h>
#include <Unet/Context.h>

#include <steam/steam_api.h>

namespace Unet
{
	class ServiceSteam : public Service
	{
	private:
		Context* m_ctx;

		MultiCallback<CreateLobbyResult>::ServiceRequest* m_requestLobbyCreated = nullptr;
		CCallResult<ServiceSteam, LobbyCreated_t> m_callLobbyCreated;

		CCallback<ServiceSteam, LobbyKicked_t> m_callLobbyKicked;

	public:
		ServiceSteam(Context* ctx);
		virtual ~ServiceSteam();

		virtual ServiceType GetType() override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) override;
		virtual void LeaveLobby() override;

	private:
		void OnLobbyCreated(LobbyCreated_t* result, bool bIOFailure);

		void OnLobbyKicked(LobbyKicked_t* result);
	};
}
