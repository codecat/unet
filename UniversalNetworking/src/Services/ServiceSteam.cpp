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

	SteamMatchmaking()->LeaveLobby((uint64)entryPoint->ID);

	auto serviceRequest = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);
	serviceRequest->Code = Result::OK;
	serviceRequest->Data->Reason = Unet::LeaveReason::UserLeave;
}

std::string Unet::ServiceSteam::GetLobbyData(uint64_t lobbyId, const char* name)
{
	return SteamMatchmaking()->GetLobbyData((uint64)lobbyId, name);
}

int Unet::ServiceSteam::GetLobbyDataCount(uint64_t lobbyId)
{
	return SteamMatchmaking()->GetLobbyDataCount((uint64)lobbyId);
}

Unet::LobbyData Unet::ServiceSteam::GetLobbyData(uint64_t lobbyId, int index)
{
	char szKey[512];
	char szValue[512];

	LobbyData ret;
	if (SteamMatchmaking()->GetLobbyDataByIndex((uint64)lobbyId, index, szKey, 512, szValue, 512)) {
		ret.Name = szKey;
		ret.Value = szValue;
	}

	return ret;
}

void Unet::ServiceSteam::SetLobbyData(const char* name, const char* value)
{
	auto currentLobby = m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto entry = currentLobby->GetInfo().GetEntryPoint(ServiceType::Steam);
	if (entry == nullptr) {
		return;
	}

	SteamMatchmaking()->SetLobbyData((uint64)entry->ID, name, value);
}

void Unet::ServiceSteam::OnLobbyCreated(LobbyCreated_t* result, bool bIOFailure)
{
	if (bIOFailure) {
		m_requestLobbyCreated->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug("[Steam] IO Failure while creating lobby");
		return;
	}

	if (result->m_eResult != k_EResultOK) {
		m_requestLobbyCreated->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Failed creating lobby, error %d", (int)result->m_eResult));
		return;
	}

	ServiceEntryPoint newEntryPoint;
	newEntryPoint.Service = GetType();
	newEntryPoint.ID = result->m_ulSteamIDLobby;
	m_requestLobbyCreated->Data->CreatedLobby->AddEntryPoint(newEntryPoint);

	m_requestLobbyCreated->Code = Result::OK;

	m_ctx->GetCallbacks()->OnLogDebug("[Steam] Lobby created");
}

void Unet::ServiceSteam::OnLobbyList(LobbyMatchList_t* result, bool bIOFailure)
{
	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Lobby list received (%d)", (int)result->m_nLobbiesMatching));

	m_listDataFetch.clear();

	if (result->m_nLobbiesMatching == 0) {
		m_requestLobbyList->Code = Result::OK;
		return;
	}

	for (int i = 0; i < (int)result->m_nLobbiesMatching; i++) {
		CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
		if (!lobbyId.IsValid()) {
			continue;
		}

		if (SteamMatchmaking()->RequestLobbyData(lobbyId)) {
			m_listDataFetch.emplace_back(lobbyId.ConvertToUint64());
		}
	}

	if (m_listDataFetch.size() == 0) {
		m_requestLobbyList->Code = Result::Error;
	}
}

void Unet::ServiceSteam::LobbyListDataUpdated()
{
	if (m_listDataFetch.size() == 0) {
		m_requestLobbyList->Code = Result::OK;
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

			xg::Guid unetGuid(SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "unet-guid"));
			if (!unetGuid.isValid()) {
				m_ctx->GetCallbacks()->OnLogDebug("[Steam] unet-guid is not valid!");
				LobbyListDataUpdated();
				return;
			}

			ServiceEntryPoint newEntryPoint;
			newEntryPoint.Service = ServiceType::Steam;
			newEntryPoint.ID = result->m_ulSteamIDLobby;
			m_requestLobbyList->Data->AddEntryPoint(unetGuid, newEntryPoint);

			LobbyListDataUpdated();

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
