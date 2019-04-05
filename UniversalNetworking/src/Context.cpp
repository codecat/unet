#include <Unet_common.h>
#include <Unet/Context.h>
#include <Unet/Service.h>
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
		result.Code = Unet::Result::OK;
	} else {
		result.Code = Unet::Result::Error;
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
			result.Code = Result::OK;
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
	default: assert(false);
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
	newLobbyInfo.UnetGuid = xg::newGuid();
	if (name != nullptr) {
		newLobbyInfo.Name = name;
	}
	result.CreatedLobby = new Lobby(this, newLobbyInfo);

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
		m_callbacks->OnLogWarn("Can't join new lobby while still in a lobby!");
		return;
	}

	m_status = ContextStatus::Connecting;

	m_callbackLobbyJoin.Begin();

	auto &result = m_callbackLobbyJoin.GetResult();
	result.JoinedLobby = new Lobby(this, lobbyInfo);

	for (auto service : m_services) {
		auto entry = lobbyInfo.GetEntryPoint(service->GetType());
		if (entry != nullptr) {
			service->JoinLobby(entry->ID);
		}
	}
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

int Unet::Context::GetLobbyMaxPlayers(const LobbyInfo &lobbyInfo)
{
	std::vector<std::pair<ServiceType, int>> items;

	for (auto service : m_services) {
		auto entry = lobbyInfo.GetEntryPoint(service->GetType());
		if (entry == nullptr) {
			continue;
		}

		int maxPlayers = service->GetLobbyMaxPlayers(entry->ID);
		items.emplace_back(std::make_pair(entry->Service, maxPlayers));
	}

	ServiceType lowestService = ServiceType::None;
	int lowest = 0;

	for (auto &pair : items) {
		if (lowest > 0 && pair.second != lowest) {
			m_callbacks->OnLogWarn(strPrintF("Max players is different between service %s and %s! (%d and %d)",
				GetServiceNameByType(lowestService), GetServiceNameByType(pair.first),
				lowest, pair.second
			));
		}

		if (lowest == 0 || pair.second < lowest) {
			lowestService = pair.first;
			lowest = pair.second;
		}
	}

	return lowest;
}

std::string Unet::Context::GetLobbyData(const LobbyInfo &lobbyInfo, const char* name)
{
	ServiceType firstService;
	std::string ret;

	for (size_t i = 0; i < lobbyInfo.EntryPoints.size(); i++) {
		auto &entry = lobbyInfo.EntryPoints[i];

		auto service = GetService(entry.Service);
		if (service == nullptr) {
			continue;
		}

		std::string str = service->GetLobbyData(entry.ID, name);
		if (str == "") {
			continue;
		}

		if (i == 0) {
			firstService = entry.Service;
			ret = str;
		} else if (ret != str) {
			m_callbacks->OnLogWarn(strPrintF("Data \"%s\" is different between service %s and %s! (\"%s\" and \"%s\")",
				name,
				GetServiceNameByType(firstService), GetServiceNameByType(entry.Service),
				ret.c_str(), str.c_str()
			));
		}
	}

	return ret;
}

std::vector<Unet::LobbyData> Unet::Context::GetLobbyData(const LobbyInfo &lobbyInfo)
{
	std::vector<std::pair<ServiceType, LobbyData>> items;

	for (auto &entry : lobbyInfo.EntryPoints) {
		auto service = GetService(entry.Service);
		if (service == nullptr) {
			continue;
		}

		int numData = service->GetLobbyDataCount(entry.ID);
		for (int i = 0; i < numData; i++) {
			auto data = service->GetLobbyData(entry.ID, i);

			auto it = std::find_if(items.begin(), items.end(), [&data](const std::pair<ServiceType, LobbyData> &d) {
				return d.second.Name == data.Name;
			});

			if (it == items.end()) {
				items.emplace_back(std::make_pair(entry.Service, data));
			} else {
				if (it->second.Value != data.Value) {
					m_callbacks->OnLogWarn(strPrintF("Data \"%s\" is different between service %s and %s! (\"%s\" and \"%s\")",
						data.Name.c_str(),
						GetServiceNameByType(it->first), GetServiceNameByType(entry.Service),
						it->second.Value.c_str(), data.Value.c_str()
					));
				}
			}
		}
	}

	std::vector<LobbyData> ret;
	for (auto &pair : items) {
		ret.emplace_back(pair.second);
	}
	return ret;
}

Unet::Lobby* Unet::Context::CurrentLobby()
{
	return m_currentLobby;
}

Unet::Service* Unet::Context::GetService(ServiceType type)
{
	for (auto service : m_services) {
		if (service->GetType() == type) {
			return service;
		}
	}
	return nullptr;
}

void Unet::Context::OnLobbyCreated(const CreateLobbyResult &result)
{
	if (result.Code != Result::OK) {
		m_status = ContextStatus::Idle;
		LeaveLobby();
	} else {
		m_status = ContextStatus::Connected;
		m_currentLobby = result.CreatedLobby;

		auto &lobbyInfo = m_currentLobby->GetInfo();
		auto unetGuid = lobbyInfo.UnetGuid.str();

		m_currentLobby->SetData("unet-guid", unetGuid.c_str());
		m_currentLobby->SetData("unet-name", lobbyInfo.Name.c_str());
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyCreated(result);
	}
}

void Unet::Context::OnLobbyList(const LobbyListResult &result)
{
	LobbyListResult newResult(result);

	for (auto &lobbyInfo : newResult.Lobbies) {
		lobbyInfo.MaxPlayers = GetLobbyMaxPlayers(lobbyInfo);
		lobbyInfo.Name = GetLobbyData(lobbyInfo, "unet-name");
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyList(newResult);
	}
}

void Unet::Context::OnLobbyJoined(const LobbyJoinResult &result)
{
	if (result.Code != Result::OK) {
		m_status = ContextStatus::Idle;
		LeaveLobby();
	} else {
		m_status = ContextStatus::Connected;
		m_currentLobby = result.JoinedLobby;
	}

	//TODO: Send all members a "hello" message

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
