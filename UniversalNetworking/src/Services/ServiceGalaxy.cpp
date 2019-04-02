#include <UNet.h>
#include <Services/ServiceGalaxy.h>

Unet::LobbyCreatedListener::LobbyCreatedListener(ServiceGalaxy* self)
{
	m_self = self;
}

void Unet::LobbyCreatedListener::OnLobbyCreated(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyCreateResult result)
{
	if (result != galaxy::api::LOBBY_CREATE_RESULT_SUCCESS) {
		m_self->m_requestLobbyCreated->Result = Result::Error;
		printf("[Galaxy] Error %d while creating lobby\n", (int)result);
		return;
	}

	auto &info = m_self->m_requestLobbyCreated->Data->Lobby->GetInfo();
	galaxy::api::Matchmaking()->SetLobbyData(lobbyID, "name", info.Name.c_str());

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = m_self->GetType();
	newEntryPoint.ID = lobbyID.ToUint64();
	m_self->m_requestLobbyCreated->Data->Lobby->AddEntryPoint(newEntryPoint);

	m_self->m_requestLobbyCreated->Result = Result::OK;

	printf("[Galaxy] Lobby created\n");
}

Unet::LobbyLeftListener::LobbyLeftListener(ServiceGalaxy* self)
{
	m_self = self;
}

void Unet::LobbyLeftListener::OnLobbyLeft(const galaxy::api::GalaxyID& lobbyID, LobbyLeaveReason leaveReason)
{
	auto currentLobby = m_self->m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto &lobbyInfo = currentLobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(ServiceType::Galaxy);
	if (entryPoint == nullptr) {
		return;
	}

	if (entryPoint->ID != lobbyID.ToUint64()) {
		return;
	}

	if (leaveReason == LOBBY_LEAVE_REASON_USER_LEFT) {
		m_self->m_requestLobbyLeft->Result = Result::OK;
		m_self->m_requestLobbyLeft->Data->Reason = LeaveReason::UserLeave;
	} else {
		currentLobby->ServiceDisconnected(ServiceType::Galaxy);
	}
}

Unet::ServiceGalaxy::ServiceGalaxy(Context* ctx) :
	m_lobbyCreatedListener(this),
	m_lobbyLeftListener(this)
{
	m_ctx = ctx;

	galaxy::api::ListenerRegistrar()->Register(LobbyLeftListener::GetListenerType(), &m_lobbyLeftListener);
}

Unet::ServiceGalaxy::~ServiceGalaxy()
{
	galaxy::api::ListenerRegistrar()->Unregister(LobbyLeftListener::GetListenerType(), &m_lobbyLeftListener);
}

Unet::ServiceType Unet::ServiceGalaxy::GetType()
{
	return ServiceType::Galaxy;
}

void Unet::ServiceGalaxy::CreateLobby(LobbyPrivacy privacy, int maxPlayers)
{
	galaxy::api::LobbyType type = galaxy::api::LOBBY_TYPE_PUBLIC;
	switch (privacy) {
	case LobbyPrivacy::Public: type = galaxy::api::LOBBY_TYPE_PUBLIC; break;
	case LobbyPrivacy::Private: type = galaxy::api::LOBBY_TYPE_PRIVATE; break;
	}

	m_requestLobbyCreated = m_ctx->m_callbackCreateLobby.AddServiceRequest(this);

	try {
		galaxy::api::Matchmaking()->CreateLobby(type, maxPlayers, true, galaxy::api::LOBBY_TOPOLOGY_TYPE_FCM, &m_lobbyCreatedListener);
	} catch (const galaxy::api::IError &error) {
		m_requestLobbyCreated->Result = Result::Error;
		printf("[Galaxy] Failed to create lobby: %s\n", error.GetMsg());
	}
}

void Unet::ServiceGalaxy::LeaveLobby()
{
	auto lobby = m_ctx->CurrentLobby();
	if (lobby == nullptr) {
		return;
	}

	auto &lobbyInfo = lobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(Unet::ServiceType::Galaxy);
	if (entryPoint == nullptr) {
		return;
	}

	m_requestLobbyLeft = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);

	try {
		galaxy::api::Matchmaking()->LeaveLobby(entryPoint->ID, &m_lobbyLeftListener);
	} catch (const galaxy::api::IError &error) {
		m_requestLobbyLeft->Result = Result::Error;
		printf("[Galaxy] Failed to leave lobby: %s\n", error.GetMsg());
	}
}
