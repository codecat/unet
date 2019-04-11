#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/Utils.h>
#include <Unet/Context.h>

Unet::LobbyMember::LobbyMember(Context* ctx)
{
	m_ctx = ctx;
}

Unet::ServiceID Unet::LobbyMember::GetServiceID(ServiceType type) const
{
	for (auto &id : IDs) {
		if (id.Service == type) {
			return id;
		}
	}
	return ServiceID();
}

Unet::ServiceID Unet::LobbyMember::GetDataServiceID() const
{
	assert(IDs.size() > 0);

	// Prefer our primary service, if the client supports it
	for (auto &id : IDs) {
		if (id.Service == m_ctx->m_primaryService) {
			return id;
		}
	}

	// Prefer the client's primary service, if we support it
	for (auto &id : IDs) {
		if (id.Service == UnetPrimaryService) {
			if (m_ctx->GetService(id.Service) != nullptr) {
				return id;
			}
			break;
		}
	}

	// As a fallback, just pick any ID that we support
	for (auto &id : IDs) {
		if (m_ctx->GetService(id.Service) != nullptr) {
			return id;
		}
	}

	// We can't send messages to this member
	return ServiceID();
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

Unet::ServiceID Unet::Lobby::GetPrimaryEntryPoint()
{
	if (m_info.EntryPoints.size() == 0) {
		return ServiceID();
	}

	auto primaryEntryPoint = m_info.GetEntryPoint(m_ctx->m_primaryService);
	if (primaryEntryPoint != nullptr) {
		return *primaryEntryPoint;
	}

	return m_info.EntryPoints[0];
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

Unet::LobbyMember* Unet::Lobby::GetHostMember()
{
	return GetMember(0);
}

void Unet::Lobby::AddEntryPoint(const ServiceID &entryPoint)
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
	auto it = std::find_if(m_info.EntryPoints.begin(), m_info.EntryPoints.end(), [service](const ServiceID & entryPoint) {
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

		//TODO: Run some callback and close the lobby?
	}
}

Unet::LobbyMember &Unet::Lobby::AddMemberService(const xg::Guid &guid, const ServiceID &id)
{
	for (size_t i = 0; i < m_members.size(); i++) {
		bool foundMember = false;
		auto &member = m_members[i];

		if (member.UnetGuid == guid) {
			continue;
		}

		for (auto &memberId : member.IDs) {
			if (memberId != id) {
				continue;
			}

			auto strGuid = guid.str();
			auto strExistingGuid = member.UnetGuid.str();

			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding %s ID 0x%08llX to member with guid %s, but another member with guid %s already has this ID! Assuming existing member is no longer connected, removing from member list.",
				GetServiceNameByType(id.Service), id.ID,
				strGuid.c_str(), strExistingGuid.c_str()
			));

			m_members.erase(m_members.begin() + i);
			foundMember = true;
			break;
		}

		if (foundMember) {
			break;
		}
	}

	for (auto &member : m_members) {
		if (member.UnetGuid == guid) {
			auto existingId = member.GetServiceID(id.Service);
			if (existingId.IsValid()) {
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

	LobbyMember newMember(m_ctx);
	newMember.Valid = false;
	newMember.UnetGuid = guid;
	newMember.UnetPeer = m_members.size();
	newMember.IDs.emplace_back(id);
	m_members.emplace_back(newMember);

	return m_members[m_members.size() - 1];
}

void Unet::Lobby::RemoveMemberService(const ServiceID &id)
{
	auto member = GetMember(id);
	if (member == nullptr) {
		return;
	}

	auto it = std::find(member->IDs.begin(), member->IDs.end(), id);
	assert(it != member->IDs.end());
	if (it == member->IDs.end()) {
		return;
	}

	member->IDs.erase(it);
	if (member->IDs.size() == 0) {
		LobbyMember callbackCopy = *member;

		size_t i = member - m_members.data();
		m_members.erase(m_members.begin() + i);

		m_ctx->OnLobbyPlayerLeft(callbackCopy);
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
