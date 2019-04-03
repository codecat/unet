#include <Unet_common.h>
#include <Unet/Services/ServiceSteam.h>
#include <Unet/Utils.h>

Unet::ServiceSteam::ServiceSteam(Context* ctx) :
	m_callLobbyDataUpdate(this, &ServiceSteam::OnLobbyDataUpdate),
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

void Unet::ServiceSteam::GetLobbyList()
{
	SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();
	m_requestLobbyList = m_ctx->m_callbackLobbyList.AddServiceRequest(this);
	m_callLobbyList.Set(call, this, &ServiceSteam::OnLobbyList);
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
		m_ctx->GetCallbacks()->OnLogDebug("[Steam] IO Failure while creating lobby");
		return;
	}

	if (result->m_eResult != k_EResultOK) {
		m_requestLobbyCreated->Result = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Failed creating lobby, error %d", (int)result->m_eResult));
		return;
	}

	auto &lobbyInfo = m_requestLobbyCreated->Data->Lobby->GetInfo();
	SteamMatchmaking()->SetLobbyData(result->m_ulSteamIDLobby, "name", lobbyInfo.Name.c_str());

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = GetType();
	newEntryPoint.ID = result->m_ulSteamIDLobby;
	m_requestLobbyCreated->Data->Lobby->AddEntryPoint(newEntryPoint);

	m_requestLobbyCreated->Result = Result::OK;

	m_ctx->GetCallbacks()->OnLogDebug("[Steam] Lobby created");
}

void Unet::ServiceSteam::OnLobbyList(LobbyMatchList_t* result, bool bIOFailure)
{
	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Lobby list received (%d)", (int)result->m_nLobbiesMatching));

	if (result->m_nLobbiesMatching == 0) {
		m_requestLobbyList->Result = Result::OK;
		return;
	}

	int numDataRequested = 0;
	for (int i = 0; i < (int)result->m_nLobbiesMatching; i++) {
		CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
		if (!lobbyId.IsValid()) {
			continue;
		}

		if (SteamMatchmaking()->RequestLobbyData(lobbyId)) {
			numDataRequested++;
		}
	}

	if (numDataRequested == 0) {
		m_requestLobbyList->Result = Result::Error;
	}
}

void Unet::ServiceSteam::OnLobbyDataUpdate(LobbyDataUpdate_t* result)
{
	if (result->m_ulSteamIDMember == result->m_ulSteamIDLobby) {
		// Lobby data

		auto it = std::find(m_listDataFetch.begin(), m_listDataFetch.end(), result->m_ulSteamIDLobby);
		if (it != m_listDataFetch.end()) {
			// Server list data request
			m_listDataFetch.erase(it);

			//TODO: Match unique Unet ID of existing lobbies, and then only add entrypoint to it
			LobbyInfo newLobbyInfo;
			newLobbyInfo.MaxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(result->m_ulSteamIDLobby);
			newLobbyInfo.Name = SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "name");

			ServiceEntryPoint newEntryPoint;
			newEntryPoint.Service = ServiceType::Steam;
			newEntryPoint.ID = result->m_ulSteamIDLobby;
			newLobbyInfo.EntryPoints.emplace_back(newEntryPoint);

			m_requestLobbyList->Data->Lobbies.emplace_back(newLobbyInfo);

		} else {
			// Regular lobby data update

		}

	} else {
		// Member data

	}
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
