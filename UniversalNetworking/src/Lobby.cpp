#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/Context.h>
#include <Unet/LobbyPacket.h>

Unet::Lobby::Lobby(Context* ctx, const LobbyInfo &lobbyInfo)
{
	m_ctx = ctx;
	m_info = lobbyInfo;
	m_info.EntryPoints.clear();
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
			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding an entry point for service %s that already exists, with different ID's! Old: 0x%016llX, new: 0x%016llX. Keeping old!",
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

	for (auto &member : m_members) {
		for (int i = (int)member.IDs.size() - 1; i >= 0; i--) {
			if (member.IDs[i].Service == service) {
				RemoveMemberService(member.IDs[i]);
			}
		}
	}

	if (IsConnected()) {
		m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Lost connection to entry point %s (%d points still open)", GetServiceNameByType(service), (int)m_info.EntryPoints.size()));
	} else {
		m_ctx->GetCallbacks()->OnLogError("Lost connection to all entry points!");

		LobbyLeftResult result;
		result.Code = Result::OK;
		result.Reason = LeaveReason::Disconnected;
		m_ctx->OnLobbyLeft(result);
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

			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding %s ID 0x%016llX to member with guid %s, but another member with guid %s already has this ID! Assuming existing member is no longer connected, removing from member list.",
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
	newMember.UnetPeer = GetNextAvailablePeer();
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

void Unet::Lobby::RemoveMember(const LobbyMember &member)
{
	LobbyMember callbackCopy = member;

	size_t i = &member - m_members.data();
	m_members.erase(m_members.begin() + i);

	m_ctx->OnLobbyPlayerLeft(callbackCopy);
}

void Unet::Lobby::SetData(const std::string &name, const std::string &value)
{
	LobbyDataContainer::SetData(name, value);

	if (m_info.IsHosting) {
		for (auto &entry : m_info.EntryPoints) {
			auto service = m_ctx->GetService(entry.Service);
			if (service != nullptr) {
				service->SetLobbyData(entry, name.c_str(), value.c_str());
			}
		}

		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyData;
		js["name"] = name;
		js["value"] = value;
		m_ctx->InternalSendToAll(js);
	}
}

std::string Unet::Lobby::GetData(const std::string &name) const
{
	std::string ret = LobbyDataContainer::GetData(name);
	if (ret != "") {
		return ret;
	}

	ServiceType firstService = ServiceType::None;

	for (size_t i = 0; i < m_info.EntryPoints.size(); i++) {
		auto &entry = m_info.EntryPoints[i];

		auto service = m_ctx->GetService(entry.Service);
		if (service == nullptr) {
			continue;
		}

		std::string str = service->GetLobbyData(entry, name.c_str());
		if (str == "") {
			continue;
		}

		if (i == 0) {
			firstService = entry.Service;
			ret = str;
		} else if (ret != str) {
			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Data \"%s\" is different between service %s and %s! (\"%s\" and \"%s\")",
				name.c_str(),
				GetServiceNameByType(firstService), GetServiceNameByType(entry.Service),
				ret.c_str(), str.c_str()
			));
		}
	}

	return ret;
}

void Unet::Lobby::RemoveData(const std::string &name)
{
	LobbyDataContainer::RemoveData(name);

	if (m_info.IsHosting) {
		for (auto &entry : m_info.EntryPoints) {
			auto service = m_ctx->GetService(entry.Service);
			if (service != nullptr) {
				service->RemoveLobbyData(entry, name.c_str());
			}
		}

		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyDataRemoved;
		js["name"] = name;
		m_ctx->InternalSendToAll(js);
	}
}

int Unet::Lobby::GetNextAvailablePeer()
{
	int i = 0;
	while (GetMember(i) != nullptr) {
		i++;
	}
	return i;
}
