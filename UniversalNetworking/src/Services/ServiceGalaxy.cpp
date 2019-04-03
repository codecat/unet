#include <Unet_common.h>
#include <Unet/Services/ServiceGalaxy.h>
#include <Unet/Utils.h>

void Unet::LobbyCreatedListener::OnLobbyCreated(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyCreateResult result)
{
	if (result != galaxy::api::LOBBY_CREATE_RESULT_SUCCESS) {
		m_self->m_requestLobbyCreated->Result = Result::Error;
		m_self->m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Error %d while creating lobby", (int)result));
		return;
	}

	auto &info = m_self->m_requestLobbyCreated->Data->Lobby->GetInfo();
	galaxy::api::Matchmaking()->SetLobbyData(lobbyID, "name", info.Name.c_str());

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = m_self->GetType();
	newEntryPoint.ID = lobbyID.ToUint64();
	m_self->m_requestLobbyCreated->Data->Lobby->AddEntryPoint(newEntryPoint);

	m_self->m_requestLobbyCreated->Result = Result::OK;

	m_self->m_ctx->GetCallbacks()->OnLogDebug("[Galaxy] Lobby created");
}

void Unet::LobbyListListener::OnLobbyList(uint32_t lobbyCount, galaxy::api::LobbyListResult result)
{
	m_self->m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Lobby list received (%d)", (int)lobbyCount));

	m_dataFetch.clear();

	if (result != galaxy::api::LOBBY_LIST_RESULT_SUCCESS) {
		m_self->m_requestLobbyList->Result = Result::Error;
		m_self->m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Couldn't get lobby list due to error %d", (int)result));
		return;
	}

	if (lobbyCount == 0) {
		m_self->m_requestLobbyList->Result = Result::OK;
		return;
	}

	for (uint32_t i = 0; i < lobbyCount; i++) {
		auto lobbyId = galaxy::api::Matchmaking()->GetLobbyByIndex(i);
		if (!lobbyId.IsValid()) {
			continue;
		}

		try {
			galaxy::api::Matchmaking()->RequestLobbyData(lobbyId, this);
			m_dataFetch.emplace_back(lobbyId);
		} catch (const galaxy::api::IError &error) {
			m_self->m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Couldn't get lobby data: %s", error.GetMsg()));
		}
	}

	if (m_dataFetch.size() == 0) {
		m_self->m_requestLobbyList->Result = Result::Error;
	}
}

void Unet::LobbyListListener::LobbyDataUpdated()
{
	if (m_dataFetch.size() == 0) {
		m_self->m_requestLobbyList->Result = Result::OK;
	}
}

void Unet::LobbyListListener::OnLobbyDataRetrieveSuccess(const galaxy::api::GalaxyID& lobbyID)
{
	auto it = std::find(m_dataFetch.begin(), m_dataFetch.end(), lobbyID);
	if (it != m_dataFetch.end()) {
		m_dataFetch.erase(it);
	}

	//TODO: Match unique Unet ID of existing lobbies, and then only add entrypoint to it

	LobbyInfo newLobbyInfo;
	newLobbyInfo.MaxPlayers = galaxy::api::Matchmaking()->GetMaxNumLobbyMembers(lobbyID);
	newLobbyInfo.Name = galaxy::api::Matchmaking()->GetLobbyData(lobbyID, "name");

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = ServiceType::Galaxy;
	newEntryPoint.ID = lobbyID.ToUint64();
	newLobbyInfo.EntryPoints.emplace_back(newEntryPoint);

	m_self->m_requestLobbyList->Data->Lobbies.emplace_back(newLobbyInfo);

	LobbyDataUpdated();
}

void Unet::LobbyListListener::OnLobbyDataRetrieveFailure(const galaxy::api::GalaxyID& lobbyID, FailureReason failureReason)
{
	auto it = std::find(m_dataFetch.begin(), m_dataFetch.end(), lobbyID);
	if (it != m_dataFetch.end()) {
		m_dataFetch.erase(it);
	}

	m_self->m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Failed to retrieve lobby data, error %d", (int)failureReason));

	LobbyDataUpdated();
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
	m_lobbyListListener(this),
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
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Failed to create lobby: %s", error.GetMsg()));
	}
}

void Unet::ServiceGalaxy::GetLobbyList()
{
	m_requestLobbyList = m_ctx->m_callbackLobbyList.AddServiceRequest(this);

	try {
		galaxy::api::Matchmaking()->RequestLobbyList(false, &m_lobbyListListener);
	} catch (const galaxy::api::IError &error) {
		m_requestLobbyList->Result = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Failed to list lobbies: %s", error.GetMsg()));
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
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Galaxy] Failed to leave lobby: %s", error.GetMsg()));
	}
}

std::string Unet::ServiceGalaxy::GetLobbyData(uint64_t lobbyId, const char* name)
{
	return galaxy::api::Matchmaking()->GetLobbyData(lobbyId, name);
}

int Unet::ServiceGalaxy::GetLobbyDataCount(uint64_t lobbyId)
{
	return galaxy::api::Matchmaking()->GetLobbyDataCount(lobbyId);
}

Unet::LobbyData Unet::ServiceGalaxy::GetLobbyData(uint64_t lobbyId, int index)
{
	char szKey[512];
	char szValue[512];

	LobbyData ret;
	if (galaxy::api::Matchmaking()->GetLobbyDataByIndex(lobbyId, index, szKey, 512, szValue, 512)) {
		ret.Name = szKey;
		ret.Value = szValue;
	}

	return ret;
}
