#include <UNet.h>
#include <Services/ServiceSteam.h>
#include <Services/ServiceGalaxy.h>

#include <algorithm>

const Unet::ServiceEntryPoint* Unet::LobbyInfo::GetEntryPoint(Unet::ServiceType service) const
{
	for (auto &entryPoint : EntryPoints) {
		if (entryPoint.Service == service) {
			return &entryPoint;
		}
	}
	return nullptr;
}

Unet::Lobby::Lobby(const LobbyInfo &lobbyInfo)
{
	m_info = lobbyInfo;
}

Unet::Lobby::~Lobby()
{
}

const Unet::LobbyInfo &Unet::Lobby::GetInfo()
{
	return m_info;
}

bool Unet::Lobby::IsConnected()
{
	return m_info.EntryPoints.size() > 0;
}

void Unet::Lobby::AddEntryPoint(ServiceEntryPoint entryPoint)
{
	m_info.EntryPoints.emplace_back(entryPoint);
}

void Unet::Lobby::ServiceDisconnected(ServiceType service)
{
	auto it = std::find_if(m_info.EntryPoints.begin(), m_info.EntryPoints.end(), [service](const ServiceEntryPoint &entryPoint) {
		return entryPoint.Service == service;
	});

	if (it == m_info.EntryPoints.end()) {
		return;
	}

	m_info.EntryPoints.erase(it);

	if (IsConnected()) {
		printf("Lost connection to entry point %s (%d points still open)\n", GetServiceNameByType(service), (int)m_info.EntryPoints.size());
	} else {
		printf("Lost connection to all entry points\n");
	}
}

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
		printf("There were some errors in the multi-service callback: %d errors\n", numRequests - numOK);
	}

	(ctx->*func)(result);

	callback.Clear();
}

void Unet::Context::SetCallbacks(Callbacks* callbacks)
{
	m_callbacks = callbacks;
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
			printf("Connection to lobby was lost (no more remaining entry points)\n");

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
		printf("Couldn't make new \"%s\" service!\n", GetServiceNameByType(service));
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
	result.Lobby = new Lobby(newLobbyInfo);

	for (auto service : m_services) {
		service->CreateLobby(privacy, maxPlayers);
	}
}

void Unet::Context::GetLobbyList()
{
	//TODO
	LobbyListResult res;
	res.Result = Result::OK;

	LobbyInfo newLobbyInfo;
	newLobbyInfo.Name = "Fake lobby Steam & Gog";
	newLobbyInfo.Privacy = LobbyPrivacy::Public;

	ServiceEntryPoint newEntryPointSteam;
	newEntryPointSteam.Service = ServiceType::Steam;
	newEntryPointSteam.ID = 0x11111111;
	newLobbyInfo.EntryPoints.emplace_back(newEntryPointSteam);

	ServiceEntryPoint newEntryPointGalaxy;
	newEntryPointGalaxy.Service = ServiceType::Galaxy;
	newEntryPointGalaxy.ID = 0x22222222;
	newLobbyInfo.EntryPoints.emplace_back(newEntryPointGalaxy);

	res.Lobbies.emplace_back(newLobbyInfo);

	OnLobbyList(res);
}

void Unet::Context::JoinLobby(LobbyInfo &lobbyInfo)
{
	if (m_status != ContextStatus::Idle) {
		LeaveLobby();
	}

	m_status = ContextStatus::Connecting;

	//TODO

	auto newLobby = new Lobby(lobbyInfo);
	m_currentLobby = newLobby;

	m_status = ContextStatus::Connected;

	LobbyJoinResult res;
	res.Result = Result::OK;
	res.Lobby = newLobby;
	OnLobbyJoined(res);
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

const char* Unet::GetVersion()
{
	return "0.00.1";
}

Unet::ServiceType Unet::GetServiceTypeByName(const char* str)
{
	if (!strcmp(str, "steam")) {
		return ServiceType::Steam;
	} else if (!strcmp(str, "galaxy")) {
		return ServiceType::Galaxy;
	}
	return ServiceType::None;
}

const char* Unet::GetServiceNameByType(ServiceType type)
{
	switch (type) {
	case ServiceType::Steam: return "steam";
	case ServiceType::Galaxy: return "galaxy";
	}
	return "none";
}
