#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/Utils.h>
#include <Unet/Context.h>

Unet::ServiceID* Unet::LobbyMember::GetServiceID(ServiceType type)
{
	for (auto &id : IDs) {
		if (id.Service == type) {
			return &id;
		}
	}
	return nullptr;
}

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

const std::vector<Unet::LobbyMember> &Unet::Lobby::GetMembers()
{
	return m_members;
}

Unet::LobbyMember* Unet::Lobby::GetMember(const xg::Guid &guid)
{
	for (auto &member : m_members) {
		if (member.UnetGuid == guid) {
			return &member;
		}
	}
	return nullptr;
}

Unet::LobbyMember* Unet::Lobby::GetMember(int peer)
{
	for (auto &member : m_members) {
		if (member.UnetPeer == peer) {
			return &member;
		}
	}
	return nullptr;
}

Unet::LobbyMember* Unet::Lobby::GetMember(const ServiceID &serviceId)
{
	for (auto &member : m_members) {
		for (auto &id : member.IDs) {
			if (id == serviceId) {
				return &member;
			}
		}
	}
	return nullptr;
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

Unet::LobbyMember &Unet::Lobby::AddMemberService(const xg::Guid &guid, const ServiceID &id)
{
	for (auto &member : m_members) {
		if (member.UnetGuid == guid) {
			auto existingId = member.GetServiceID(id.Service);
			if (existingId != nullptr) {
				auto strGuid = guid.str();
				m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding player service %s for guid %s, but it already exists!",
					GetServiceNameByType(id.Service), strGuid.c_str()
				));
			} else {
				member.IDs.emplace_back(id);
			}
			return member;
		}
	}

	LobbyMember newMember;
	newMember.UnetGuid = guid;
	newMember.UnetPeer = m_members.size();
	newMember.IDs.emplace_back(id);
	m_members.emplace_back(newMember);

	return m_members[m_members.size() - 1];
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
			service->SetLobbyData(entry, name, value.c_str());
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

	ServiceType firstService = ServiceType::None;
	std::string ret;

	for (size_t i = 0; i < m_info.EntryPoints.size(); i++) {
		auto &entry = m_info.EntryPoints[i];

		auto service = m_ctx->GetService(entry.Service);
		if (service == nullptr) {
			continue;
		}

		std::string str = service->GetLobbyData(entry, name);
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
