#pragma once

#include <UNet.h>

#include <galaxy/GalaxyApi.h>

namespace Unet
{
	class GalaxyListener
	{
	protected:
		class ServiceGalaxy* m_self;
	};

	class LobbyCreatedListener : public GalaxyListener, public galaxy::api::ILobbyCreatedListener
	{
	public:
		LobbyCreatedListener(ServiceGalaxy* self);
		virtual void OnLobbyCreated(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyCreateResult result) override;
	};

	class LobbyLeftListener : public GalaxyListener, public galaxy::api::ILobbyLeftListener
	{
	public:
		LobbyLeftListener(ServiceGalaxy* self);
		virtual void OnLobbyLeft(const galaxy::api::GalaxyID& lobbyID, LobbyLeaveReason leaveReason) override;
	};

	class ServiceGalaxy : public Service
	{
		friend class LobbyLeftListener;

	private:
		Context* m_ctx;

		LobbyCreatedListener m_lobbyCreatedListener;
		LobbyLeftListener m_lobbyLeftListener;

	public:
		MultiCallback<CreateLobbyResult>::ServiceRequest* m_requestLobbyCreated = nullptr;
		MultiCallback<LobbyLeftResult>::ServiceRequest* m_requestLobbyLeft = nullptr;

	public:
		ServiceGalaxy(Context* ctx);
		virtual ~ServiceGalaxy();

		virtual ServiceType GetType() override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) override;
		virtual void LeaveLobby() override;
	};
}
