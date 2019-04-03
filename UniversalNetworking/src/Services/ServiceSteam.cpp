#include <Unet.h>
#include <Unet/Services/ServiceSteam.h>

Unet::ServiceSteam::ServiceSteam(Context* ctx) :
	m_callLobbyKicked(this, &ServiceSteam::OnLobbyKicked)
{
	m_ctx = ctx;
}

Unet::ServiceSteam::~ServiceSteam()
{
}

Unet::ServiceType Unet::ServiceSteam::GetType()
{
	return ServiceType::Steam;
}

void Unet::ServiceSteam::CreateLobby(LobbyPrivacy privacy, int maxPlayers)
{
	ELobbyType type;
	switch (privacy) {
	case LobbyPrivacy::Public: type = k_ELobbyTypePublic; break;
	case LobbyPrivacy::Private: type = k_ELobbyTypePrivate; break;
	}

	SteamAPICall_t call = SteamMatchmaking()->CreateLobby(type, maxPlayers);
	m_requestLobbyCreated = m_ctx->m_callbackCreateLobby.AddServiceRequest(this);
	m_callLobbyCreated.Set(call, this, &ServiceSteam::OnLobbyCreated);
}

void Unet::ServiceSteam::LeaveLobby()
{
	auto lobby = m_ctx->CurrentLobby();
	if (lobby == nullptr) {
		return;
	}

	auto &lobbyInfo = lobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(GetType());
	if (entryPoint == nullptr) {
		return;
	}

	SteamMatchmaking()->LeaveLobby(entryPoint->ID);

	auto serviceRequest = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);
	serviceRequest->Result = Result::OK;
	serviceRequest->Data->Reason = Unet::LeaveReason::UserLeave;
}

void Unet::ServiceSteam::OnLobbyCreated(LobbyCreated_t* result, bool bIOFailure)
{
	if (bIOFailure) {
		m_requestLobbyCreated->Result = Result::Error;
		printf("[Steam] IO Failure while creating lobby\n");
		return;
	}

	if (result->m_eResult != k_EResultOK) {
		m_requestLobbyCreated->Result = Result::Error;
		printf("[Steam] Failed creating lobby, error %d\n", (int)result->m_eResult);
		return;
	}

	auto &lobbyInfo = m_requestLobbyCreated->Data->Lobby->GetInfo();
	SteamMatchmaking()->SetLobbyData(result->m_ulSteamIDLobby, "name", lobbyInfo.Name.c_str());

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = GetType();
	newEntryPoint.ID = result->m_ulSteamIDLobby;
	m_requestLobbyCreated->Data->Lobby->AddEntryPoint(newEntryPoint);

	m_requestLobbyCreated->Result = Result::OK;

	printf("[Steam] Lobby created\n");
}

void Unet::ServiceSteam::OnLobbyKicked(LobbyKicked_t* result)
{
	auto currentLobby = m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto &lobbyInfo = currentLobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(ServiceType::Galaxy);
	if (entryPoint == nullptr) {
		return;
	}

	if (entryPoint->ID != result->m_ulSteamIDLobby) {
		return;
	}

	currentLobby->ServiceDisconnected(ServiceType::Steam);
}
