#include <Unet_common.h>
#include <Unet/Lobby.h>
#include <Unet/Context.h>
#include <Unet/LobbyPacket.h>

Unet::Lobby::Lobby(Internal::Context* ctx, const LobbyInfo &lobbyInfo)
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

const std::vector<Unet::LobbyMember*> &Unet::Lobby::GetMembers()
{
	return m_members;
}

Unet::LobbyMember* Unet::Lobby::GetMember(const xg::Guid &guid)
{
	for (auto member : m_members) {
		if (member->UnetGuid == guid) {
			return member;
		}
	}
	return nullptr;
}

Unet::LobbyMember* Unet::Lobby::GetMember(int peer)
{
	for (auto member : m_members) {
		if (member->UnetPeer == peer) {
			return member;
		}
	}
	return nullptr;
}

Unet::LobbyMember* Unet::Lobby::GetMember(const ServiceID &serviceId)
{
	for (auto member : m_members) {
		for (auto &id : member->IDs) {
			if (id == serviceId) {
				return member;
			}
		}
	}
	return nullptr;
}

Unet::LobbyMember* Unet::Lobby::GetHostMember()
{
	return GetMember(0);
}

void Unet::Lobby::HandleMessage(const ServiceID &peer, uint8_t* data, size_t size)
{
	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("Handle lobby message of %d bytes", (int)size));

	auto peerMember = GetMember(peer);

	json js = JsonUnpack(data, size);
	if (!js.is_object() || !js.contains("t")) {
		m_ctx->GetCallbacks()->OnLogError(strPrintF("[P2P] [%s] Message from 0x%016llX is not a valid data object!", GetServiceNameByType(peer.Service), peer.ID));
		return;
	}

	auto jsDump = js.dump();
	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[P2P] [%s] Message object: \"%s\"", GetServiceNameByType(peer.Service), jsDump.c_str()));

	auto type = (LobbyPacketType)(uint8_t)js["t"];

	if (type == LobbyPacketType::Handshake) {
		if (!m_info.IsHosting) {
			return;
		}

		xg::Guid guid(js["guid"].get<std::string>());
		auto member = AddMemberService(guid, peer);

		if (member->Valid) {
			js = json::object();
			js["t"] = (uint8_t)LobbyPacketType::MemberNewService;
			js["guid"] = guid.str();
			js["service"] = (int)peer.Service;
			js["id"] = peer.ID;
			m_ctx->InternalSendToAll(js);
		}

	} else if (type == LobbyPacketType::Hello) {
		if (!m_info.IsHosting) {
			return;
		}

		auto member = GetMember(peer);
		if (member == nullptr) {
			m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Received Hello packet from %s ID 0x%016llX before receiving any handshakes!",
				GetServiceNameByType(peer.Service), peer.ID
			));
			return;
		}

		// Update member
		member->Name = js["name"].get<std::string>();
		member->UnetPrimaryService = peer.Service;
		member->Valid = true;

		// Send LobbyInfo to new member
		js = json::object();
		js["t"] = (uint8_t)LobbyPacketType::LobbyInfo;
		js["data"] = SerializeData();
		js["members"] = json::array();
		for (auto member : m_members) {
			if (member->Valid) {
				js["members"].emplace_back(member->Serialize());
			}
		}
		m_ctx->InternalSendTo(member, js);

		// Send MemberInfo to existing members
		js = member->Serialize();
		js["t"] = (uint8_t)LobbyPacketType::MemberInfo;
		m_ctx->InternalSendToAllExcept(member, js);

		// Run callback
		m_ctx->GetCallbacks()->OnLobbyPlayerJoined(member);

	} else if (type == LobbyPacketType::LobbyInfo) {
		if (m_info.IsHosting) {
			return;
		}

		DeserializeData(js["data"]);
		m_info.Name = GetData("unet-name");
		m_info.UnetGuid = xg::Guid(GetData("unet-guid"));

		for (auto &member : js["members"]) {
			DeserializeMember(member);
		}

		for (auto member : m_members) {
			if (member->UnetGuid == m_ctx->m_localGuid) {
				m_ctx->m_localPeer = member->UnetPeer;
			}
		}

		m_ctx->m_status = ContextStatus::Connected;

		LobbyJoinResult result;
		result.Code = Result::OK;
		result.JoinedLobby = this;
		m_ctx->GetCallbacks()->OnLobbyJoined(result);

	} else if (type == LobbyPacketType::MemberInfo) {
		if (m_info.IsHosting) {
			return;
		}

		auto member = DeserializeMember(js);
		m_ctx->GetCallbacks()->OnLobbyPlayerJoined(member);

	} else if (type == LobbyPacketType::MemberLeft) {
		if (m_info.IsHosting) {
			return;
		}

		xg::Guid guid(js["guid"].get<std::string>());

		auto member = GetMember(guid);
		assert(member != nullptr);
		if (member == nullptr) {
			return;
		}

		RemoveMember(member);

	} else if (type == LobbyPacketType::MemberKick) {
		if (m_info.IsHosting) {
			return;
		}

		m_ctx->LeaveLobby(LeaveReason::Kicked);

	} else if (type == LobbyPacketType::MemberNewService) {
		if (m_info.IsHosting) {
			return;
		}

		xg::Guid guid(js["guid"].get<std::string>());

		ServiceID id;
		id.Service = (ServiceType)js["service"].get<int>();
		id.ID = js["id"].get<uint64_t>();

		AddMemberService(guid, id);

	} else if (type == LobbyPacketType::LobbyData) {
		if (m_info.IsHosting) {
			return;
		}

		auto name = js["name"].get<std::string>();
		auto value = js["value"].get<std::string>();

		InternalSetData(name, value);
		m_ctx->GetCallbacks()->OnLobbyDataChanged(name);

	} else if (type == LobbyPacketType::LobbyDataRemoved) {
		if (m_info.IsHosting) {
			return;
		}

		auto name = js["name"].get<std::string>();

		InternalRemoveData(name);
		m_ctx->GetCallbacks()->OnLobbyDataChanged(name);

	} else if (type == LobbyPacketType::LobbyMemberData) {
		auto name = js["name"].get<std::string>();
		auto value = js["value"].get<std::string>();

		if (m_info.IsHosting) {
			peerMember->InternalSetData(name, value);

			js = json::object();
			js["t"] = (uint8_t)LobbyPacketType::LobbyMemberData;
			js["guid"] = peerMember->UnetGuid.str();
			js["name"] = name;
			js["value"] = value;
			m_ctx->InternalSendToAllExcept(peerMember, js);

			m_ctx->GetCallbacks()->OnLobbyMemberDataChanged(peerMember, name);

		} else {
			xg::Guid guid(js["guid"].get<std::string>());

			auto member = GetMember(guid);
			assert(member != nullptr);
			if (member == nullptr) {
				return;
			}

			member->InternalSetData(name, value);
			m_ctx->GetCallbacks()->OnLobbyMemberDataChanged(member, name);
		}

	} else if (type == LobbyPacketType::LobbyMemberDataRemoved) {
		auto name = js["name"].get<std::string>();

		if (m_info.IsHosting) {
			peerMember->InternalRemoveData(name);

			js = json::object();
			js["t"] = (uint8_t)LobbyPacketType::LobbyMemberData;
			js["guid"] = peerMember->UnetGuid.str();
			js["name"] = name;
			m_ctx->InternalSendToAllExcept(peerMember, js);

			m_ctx->GetCallbacks()->OnLobbyMemberDataChanged(peerMember, name);

		} else {
			xg::Guid guid(js["guid"].get<std::string>());

			auto member = GetMember(guid);
			assert(member != nullptr);
			if (member == nullptr) {
				return;
			}

			member->InternalRemoveData(name);
			m_ctx->GetCallbacks()->OnLobbyMemberDataChanged(member, name);
		}

	} else if (type == LobbyPacketType::LobbyFileAdded) {
		auto filename = js["filename"].get<std::string>();
		auto size = js["size"].get<size_t>();
		auto hash = js["hash"].get<uint64_t>();

		auto newFile = new LobbyFile(filename);
		newFile->Prepare(size, hash);

		if (m_info.IsHosting) {
			peerMember->Files.emplace_back(newFile);

			js = json::object();
			js["t"] = (uint8_t)LobbyPacketType::LobbyFileAdded;
			js["guid"] = peerMember->UnetGuid.str();
			js["filename"] = filename;
			js["size"] = size;
			js["hash"] = hash;
			m_ctx->InternalSendToAllExcept(peerMember, js);

		} else {
			xg::Guid guid(js["guid"].get<std::string>());

			auto member = GetMember(guid);
			assert(member != nullptr);
			if (member == nullptr) {
				delete newFile;
				return;
			}

			member->Files.emplace_back(newFile);
		}

	} else {
		m_ctx->GetCallbacks()->OnLogWarn(strPrintF("P2P packet type was not recognized: %d", (int)type));
	}
}

Unet::LobbyMember* Unet::Lobby::DeserializeMember(const json &member)
{
	xg::Guid guid(member["guid"].get<std::string>());

	for (auto &memberId : member["ids"]) {
		auto service = (Unet::ServiceType)memberId[0].get<int>();
		auto id = memberId[1].get<uint64_t>();

		AddMemberService(guid, Unet::ServiceID(service, id));
	}

	auto lobbyMember = GetMember(guid);
	assert(lobbyMember != nullptr); // If this fails, there's no service IDs given for this member

	lobbyMember->Deserialize(member);

	return lobbyMember;
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

	for (auto member : m_members) {
		for (int i = (int)member->IDs.size() - 1; i >= 0; i--) {
			if (member->IDs[i].Service == service) {
				RemoveMemberService(member->IDs[i]);
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

Unet::LobbyMember* Unet::Lobby::AddMemberService(const xg::Guid &guid, const ServiceID &id)
{
	for (size_t i = 0; i < m_members.size(); i++) {
		bool foundMember = false;
		auto member = m_members[i];

		if (member->UnetGuid == guid) {
			continue;
		}

		for (auto &memberId : member->IDs) {
			if (memberId != id) {
				continue;
			}

			auto strGuid = guid.str();
			auto strExistingGuid = member->UnetGuid.str();

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

	for (auto member : m_members) {
		if (member->UnetGuid == guid) {
			auto existingId = member->GetServiceID(id.Service);
			if (existingId.IsValid()) {
				auto strGuid = guid.str();
				m_ctx->GetCallbacks()->OnLogWarn(strPrintF("Tried adding player service %s for guid %s, but it already exists!",
					GetServiceNameByType(id.Service), strGuid.c_str()
				));
			} else {
				member->IDs.emplace_back(id);
			}
			return member;
		}
	}

	auto newMember = new LobbyMember(m_ctx);
	newMember->Valid = false;
	newMember->UnetGuid = guid;
	newMember->UnetPeer = GetNextAvailablePeer();
	newMember->IDs.emplace_back(id);
	m_members.emplace_back(newMember);

	return newMember;
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
		auto itMember = std::find(m_members.begin(), m_members.end(), member);
		if (itMember != m_members.end()) {
			m_members.erase(itMember);
		}

		m_ctx->OnLobbyPlayerLeft(member);
		delete member;
	}
}

void Unet::Lobby::RemoveMember(const LobbyMember* member)
{
	auto it = std::find(m_members.begin(), m_members.end(), member);
	assert(it != m_members.end());

	m_members.erase(it);

	m_ctx->OnLobbyPlayerLeft(member);
	delete member;
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
