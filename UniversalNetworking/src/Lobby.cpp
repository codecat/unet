#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/Utils.h>
#include <Unet/Context.h>

Unet::Lobby::Lobby(Context* ctx, const LobbyInfo &lobbyInfo)
{
	m_ctx = ctx;
	m_info = lobbyInfo;
	m_info.EntryPoints.clear();

	m_data = m_ctx->GetLobbyData(m_info);
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

void Unet::Lobby::AddEntryPoint(ServiceID entryPoint)
{
	auto entry = m_info.GetEntryPoint(entryPoint.Service);
	if (entry != nullptr) {
		if (entry->ID != entryPoint.ID) {
			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding an entry point for service %s that already exists, with different ID's! Old: 0x%08llX, new: 0x%08llX. Keeping old!",
				GetServiceNameByType(entry->Service),
				entry->ID, entryPoint.ID
			));
		}
		return;
	}
	m_info.EntryPoints.emplace_back(entryPoint);
}

void Unet::Lobby::ServiceDisconnected(ServiceType service)
{
	auto it = std::find_if(m_info.EntryPoints.begin(), m_info.EntryPoints.end(), [service](const ServiceID &entryPoint) {
		return entryPoint.Service == service;
	});

	if (it == m_info.EntryPoints.end()) {
		return;
	}

	m_info.EntryPoints.erase(it);

	if (IsConnected()) {
		m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Lost connection to entry point %s (%d points still open)", GetServiceNameByType(service), (int)m_info.EntryPoints.size()));
	} else {
		m_ctx->GetCallbacks()->OnLogError("Lost connection to all entry points!");
	}
}

void Unet::Lobby::SetData(const char* name, const std::string &value)
{
	if (!m_info.IsHosting) {
		return;
	}

	//TODO: Consider uncommenting?
	//if (strncmp(name, "unet-", 5) != 0) {
		bool found = false;
		for (auto &data : m_data) {
			if (data.Name == name) {
				data.Value = value;
				found = true;
				break;
			}
		}

		if (!found) {
			LobbyData newData;
			newData.Name = name;
			newData.Value = value;
			m_data.emplace_back(newData);
		}
	//}

	for (auto &entry : m_info.EntryPoints) {
		auto service = m_ctx->GetService(entry.Service);
		if (service != nullptr) {
			service->SetLobbyData(entry.ID, name, value.c_str());
		}
	}
}

std::string Unet::Lobby::GetData(const char* name)
{
	for (auto &data : m_data) {
		if (data.Name == name) {
			return data.Value;
		}
	}

	ServiceType firstService;
	std::string ret;

	for (size_t i = 0; i < m_info.EntryPoints.size(); i++) {
		auto &entry = m_info.EntryPoints[i];

		auto service = m_ctx->GetService(entry.Service);
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
			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Data \"%s\" is different between service %s and %s! (\"%s\" and \"%s\")",
				name,
				GetServiceNameByType(firstService), GetServiceNameByType(entry.Service),
				ret.c_str(), str.c_str()
			));
		}
	}

	return ret;
}

const std::vector<Unet::LobbyData> &Unet::Lobby::GetData()
{
	return m_data;
}
