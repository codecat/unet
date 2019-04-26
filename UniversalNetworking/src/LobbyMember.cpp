#include <Unet_common.h>
#include <Unet/LobbyMember.h>
#include <Unet/Context.h>
#include <Unet/LobbyPacket.h>

Unet::LobbyMember::LobbyMember(Internal::Context* ctx)
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

Unet::ServiceID Unet::LobbyMember::GetPrimaryServiceID() const
{
	return GetServiceID(UnetPrimaryService);
}

json Unet::LobbyMember::Serialize() const
{
	json js;
	js["guid"] = UnetGuid.str();
	js["peer"] = UnetPeer;
	js["primary"] = (int)UnetPrimaryService;
	js["name"] = Name;
	js["ids"] = json::array();
	for (auto &id : IDs) {
		js["ids"].emplace_back(json::array({ (int)id.Service, id.ID }));
	}
	js["data"] = SerializeData();
	return js;
}

void Unet::LobbyMember::Deserialize(const json &js)
{
	Valid = true;

	UnetPeer = js["peer"].get<int>();
	UnetPrimaryService = (Unet::ServiceType)js["primary"].get<int>();
	Name = js["name"].get<std::string>();

	DeserializeData(js["data"]);
}

void Unet::LobbyMember::SetData(const std::string &name, const std::string &value)
{
	LobbyDataContainer::SetData(name, value);

	auto currentLobby = m_ctx->CurrentLobby();
	assert(currentLobby != nullptr);
	if (currentLobby != nullptr && currentLobby->GetInfo().IsHosting) {
		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyMemberData;
		js["guid"] = UnetGuid.str();
		js["name"] = name;
		js["value"] = value;
		m_ctx->InternalSendToAll(js);

	} else if (UnetPeer == m_ctx->m_localPeer) {
		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyMemberData;
		js["name"] = name;
		js["value"] = value;
		m_ctx->InternalSendToHost(js);
	}
}

void Unet::LobbyMember::RemoveData(const std::string &name)
{
	LobbyDataContainer::RemoveData(name);

	auto currentLobby = m_ctx->CurrentLobby();
	assert(currentLobby != nullptr);
	if (currentLobby != nullptr && currentLobby->GetInfo().IsHosting) {
		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyMemberDataRemoved;
		js["guid"] = UnetGuid.str();
		js["name"] = name;
		m_ctx->InternalSendToAll(js);

	} else if (UnetPeer == m_ctx->m_localPeer) {
		json js;
		js["t"] = (uint8_t)LobbyPacketType::LobbyMemberDataRemoved;
		js["name"] = name;
		m_ctx->InternalSendToHost(js);
	}
}
