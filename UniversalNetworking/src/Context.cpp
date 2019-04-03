#include <Unet_common.h>
#include <Unet/Context.h>
#include <Unet/Services/ServiceSteam.h>
#include <Unet/Services/ServiceGalaxy.h>
#include <Unet/Utils.h>

Unet::Context::Context()
{
	m_status = ContextStatus::Idle;
	m_primaryService = ServiceType::None;

	m_callbacks = nullptr;

	m_currentLobby = nullptr;
}

Unet::Context::~Context()
{
	if (m_currentLobby != nullptr) {
		//TODO: Proper leave
		delete m_currentLobby;
	}

	if (m_callbacks != nullptr) {
		delete m_callbacks;
	}
}

Unet::ContextStatus Unet::Context::GetStatus()
{
	return m_status;
}

template<typename TResult, typename TFunc>
void CheckCallback(Unet::Context* ctx, Unet::MultiCallback<TResult> &callback, TFunc func)
{
	if (!callback.Ready()) {
		return;
	}

	int numOK = callback.NumOK();
	int numRequests = callback.NumRequests();

	auto &result = callback.GetResult();
	if (numOK > 0) {
		result.Result = Unet::Result::OK;
	} else {
		result.Result = Unet::Result::Error;
	}

	if (numOK < numRequests) {
		ctx->GetCallbacks()->OnLogDebug(Unet::strPrintF("There were some errors in the multi-service callback: %d errors", numRequests - numOK));
	}

	(ctx->*func)(result);

	callback.Clear();
}

void Unet::Context::SetCallbacks(Callbacks* callbacks)
{
	m_callbacks = callbacks;
}

Unet::Callbacks* Unet::Context::GetCallbacks()
{
	return m_callbacks;
}

void Unet::Context::RunCallbacks()
{
	CheckCallback(this, m_callbackCreateLobby, &Context::OnLobbyCreated);
	CheckCallback(this, m_callbackLobbyList, &Context::OnLobbyList);
	CheckCallback(this, m_callbackLobbyJoin, &Context::OnLobbyJoined);
	CheckCallback(this, m_callbackLobbyLeft, &Context::OnLobbyLeft);

	if (m_status == ContextStatus::Connected && m_currentLobby != nullptr) {
		auto &lobbyInfo = m_currentLobby->GetInfo();
		if (lobbyInfo.EntryPoints.size() == 0) {
			m_callbacks->OnLogError("Connection to lobby was lost (no more remaining entry points)");

			LobbyLeftResult result;
			result.Result = Result::OK;
			result.Reason = LeaveReason::Disconnected;
			OnLobbyLeft(result);
		}
	}
}

void Unet::Context::SetPrimaryService(ServiceType service)
{
	m_primaryService = service;
}

void Unet::Context::EnableService(ServiceType service)
{
	Service* newService = nullptr;
	switch (service) {
	case ServiceType::Steam: newService = new ServiceSteam(this); break;
	case ServiceType::Galaxy: newService = new ServiceGalaxy(this); break;
	}

	if (newService == nullptr) {
		m_callbacks->OnLogError(strPrintF("Couldn't make new \"%s\" service!", GetServiceNameByType(service)));
		return;
	}

	if (m_primaryService == ServiceType::None) {
		m_primaryService = service;
	}

	m_services.emplace_back(newService);
}

void Unet::Context::CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name)
{
	m_status = ContextStatus::Connecting;

	m_callbackCreateLobby.Begin();

	auto &result = m_callbackCreateLobby.GetResult();
	LobbyInfo newLobbyInfo;
	newLobbyInfo.IsHosting = true;
	newLobbyInfo.Privacy = privacy;
	newLobbyInfo.MaxPlayers = maxPlayers;
	if (name != nullptr) {
		newLobbyInfo.Name = name;
	}
	result.Lobby = new Lobby(this, newLobbyInfo);

	for (auto service : m_services) {
		service->CreateLobby(privacy, maxPlayers);
	}
}

void Unet::Context::GetLobbyList()
{
	m_callbackLobbyList.Begin();

	for (auto service : m_services) {
		service->GetLobbyList();
	}
}

void Unet::Context::JoinLobby(LobbyInfo &lobbyInfo)
{
	if (m_status != ContextStatus::Idle) {
		LeaveLobby();
	}

	m_status = ContextStatus::Connecting;

	//TODO
	/*
	auto newLobby = new Lobby(lobbyInfo);
	m_currentLobby = newLobby;

	m_status = ContextStatus::Connected;

	LobbyJoinResult res;
	res.Result = Result::OK;
	res.Lobby = newLobby;
	OnLobbyJoined(res);
	*/
}

void Unet::Context::LeaveLobby()
{
	if (m_status == ContextStatus::Connected) {
		m_callbackLobbyLeft.Begin();

		for (auto service : m_services) {
			service->LeaveLobby();
		}

	} else if (m_status == ContextStatus::Connecting) {
		//TODO: Proper cancelation

	} else {
		if (m_currentLobby != nullptr) {
			delete m_currentLobby;
			m_currentLobby = nullptr;
		}

		m_status = ContextStatus::Idle;
	}
}

Unet::Lobby* Unet::Context::CurrentLobby()
{
	return m_currentLobby;
}

void Unet::Context::OnLobbyCreated(const CreateLobbyResult &result)
{
	if (result.Result != Result::OK) {
		m_status = ContextStatus::Idle;
		LeaveLobby();
	} else {
		m_status = ContextStatus::Connected;
		m_currentLobby = result.Lobby;
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyCreated(result);
	}
}

void Unet::Context::OnLobbyList(const LobbyListResult &result)
{
	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyList(result);
	}
}

void Unet::Context::OnLobbyJoined(const LobbyJoinResult &result)
{
	if (result.Result != Result::OK) {
		m_status = ContextStatus::Idle;
		LeaveLobby();
	} else {
		m_status = ContextStatus::Connected;
		m_currentLobby = result.Lobby;
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyJoined(result);
	}
}

void Unet::Context::OnLobbyLeft(const LobbyLeftResult &result)
{
	m_status = ContextStatus::Idle;

	if (m_currentLobby != nullptr) {
		delete m_currentLobby;
		m_currentLobby = nullptr;
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyLeft(result);
	}
}
