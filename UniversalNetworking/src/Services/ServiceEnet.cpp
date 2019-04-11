#include <Unet_common.h>
#include <Unet/Services/ServiceEnet.h>
#include <Unet/Utils.h>
#include <Unet/LobbyPacket.h>

#include <Unet/json.hpp>
using json = nlohmann::json;

// I seriously hate Windows.h
#if defined(min)
#undef min
#endif

#define UNET_PORT 4450
#define UNET_ID_MASK 0x0000FFFFFFFFFFFF

static uint64_t AddressToInt(const ENetAddress &addr)
{
	return *(uint64_t*)& addr & UNET_ID_MASK;
}

static Unet::ServiceID AddressToID(const ENetAddress &addr)
{
	return Unet::ServiceID(Unet::ServiceType::Enet, AddressToInt(addr));
}

static ENetAddress IDToAddress(const Unet::ServiceID &id)
{
	return *(ENetAddress*)&id.ID;
}

Unet::ServiceEnet::ServiceEnet(Context* ctx)
{
	m_ctx = ctx;
}

Unet::ServiceEnet::~ServiceEnet()
{
}

void Unet::ServiceEnet::RunCallbacks()
{
	if (m_host == nullptr) {
		return;
	}

	ENetEvent ev;
	while (enet_host_service(m_host, &ev, 0)) {
		if (ev.type == ENET_EVENT_TYPE_CONNECT) {
			if (m_requestLobbyJoin != nullptr && m_requestLobbyJoin->Code != Result::OK) {
				m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Enet] Connection to host established: %08llX", AddressToInt(ev.peer->address)));

				m_requestLobbyJoin->Code = Result::OK;
				m_requestLobbyJoin->Data->JoinedLobby->AddEntryPoint(AddressToID(ev.peer->address));

				json js;
				js["t"] = (uint8_t)LobbyPacketType::Handshake;
				js["guid"] = m_requestLobbyJoin->Data->JoinGuid.str();
				std::vector<uint8_t> msg = json::to_bson(js);

				ENetPacket* newPacket = enet_packet_create(msg.data(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(m_peerHost, 0, newPacket);

			} else {
				m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Enet] Client connected: %08llX", AddressToInt(ev.peer->address)));
			}

		} else if (ev.type == ENET_EVENT_TYPE_DISCONNECT) {
			if (m_requestLobbyLeft != nullptr && m_requestLobbyLeft->Code != Result::OK) {
				m_ctx->GetCallbacks()->OnLogDebug("[Enet] Disconnected from host");

				enet_host_destroy(m_host);
				m_host = nullptr;
				m_peerHost = nullptr;

				m_requestLobbyLeft->Code = Result::OK;

			} else {
				m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Enet] Client disconnected: %08llX", AddressToInt(ev.peer->address)));

				auto currentLobby = m_ctx->CurrentLobby();
				if (currentLobby != nullptr) {
					currentLobby->RemoveMemberService(AddressToID(ev.peer->address));
				}
			}

		} else if (ev.type == ENET_EVENT_TYPE_RECEIVE) {
			if (ev.channelID >= m_channels.size()) {
				m_ctx->GetCallbacks()->OnLogWarn(strPrintF("[Enet] Ignoring packet with %d bytes received in out-of-range channel ID %d", (int)ev.packet->dataLength, (int)ev.channelID));
				enet_packet_destroy(ev.packet);
				continue;
			}

			m_channels[ev.channelID].push(ev.packet);
		}
	}
}

Unet::ServiceType Unet::ServiceEnet::GetType()
{
	return ServiceType::Enet;
}

Unet::ServiceID Unet::ServiceEnet::GetUserID()
{
	//TODO: Use local IP or something
	return ServiceID(ServiceType::Enet, 0);
}

std::string Unet::ServiceEnet::GetUserName()
{
	//TODO: Use Windows name or something
	return "";
}

void Unet::ServiceEnet::CreateLobby(LobbyPrivacy privacy, int maxPlayers)
{
	ENetAddress addr;
	addr.host = ENET_HOST_ANY;
	addr.port = UNET_PORT;

	size_t maxChannels = 3; //TODO: Make this customizable (minimum is 3!)

	Clear(maxChannels);

	m_host = enet_host_create(&addr, maxPlayers, maxChannels, 0, 0);
	m_peerHost = nullptr;

	auto req = m_ctx->m_callbackCreateLobby.AddServiceRequest(this);
	req->Data->CreatedLobby->AddEntryPoint(AddressToID(addr));
	req->Code = Result::OK;
}

void Unet::ServiceEnet::GetLobbyList()
{
	//TODO: Broadcast to LAN

	auto req = m_ctx->m_callbackLobbyList.AddServiceRequest(this);
	req->Code = Result::OK;
}

void Unet::ServiceEnet::JoinLobby(const ServiceID &id)
{
	assert(id.Service == ServiceType::Enet);

	m_requestLobbyJoin = m_ctx->m_callbackLobbyJoin.AddServiceRequest(this);

	auto addr = IDToAddress(id);
	size_t maxChannels = 3; //TODO: Make this customizable (minimum is 3!)

	Clear(maxChannels);

	m_host = enet_host_create(nullptr, 1, maxChannels, 0, 0);
	m_peerHost = enet_host_connect(m_host, &addr, maxChannels, 0);

	m_peers.clear();
	m_peers.emplace_back(m_peerHost);
}

void Unet::ServiceEnet::LeaveLobby()
{
	m_requestLobbyLeft = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);
	m_requestLobbyLeft->Data->Reason = LeaveReason::UserLeave;

	if (m_peerHost != nullptr) {
		enet_peer_disconnect(m_peerHost, 0);

	} else {
		enet_host_destroy(m_host);
		m_host = nullptr;

		m_requestLobbyLeft->Code = Result::OK;
	}
}

int Unet::ServiceEnet::GetLobbyMaxPlayers(const ServiceID &lobbyId)
{
	//TODO
	return m_host->peerCount;
}

Unet::ServiceID Unet::ServiceEnet::GetLobbyHost(const ServiceID &lobbyId)
{
	return AddressToID(m_peerHost->address);
}

std::string Unet::ServiceEnet::GetLobbyData(const ServiceID &lobbyId, const char* name)
{
	return "";
}

int Unet::ServiceEnet::GetLobbyDataCount(const ServiceID &lobbyId)
{
	return 0;
}

Unet::LobbyData Unet::ServiceEnet::GetLobbyData(const ServiceID &lobbyId, int index)
{
	return LobbyData();
}

void Unet::ServiceEnet::SetLobbyData(const ServiceID &lobbyId, const char* name, const char* value)
{
}

void Unet::ServiceEnet::SendPacket(const ServiceID &peerId, const void* data, size_t size, PacketType type, uint8_t channel)
{
	auto peer = GetPeer(peerId);
	if (peer == nullptr) {
		m_ctx->GetCallbacks()->OnLogWarn(strPrintF("[Enet] Tried sending packet of %d bytes to unidentified peer 0x%08llX on channel %d", (int)size, peerId.ID, (int)channel));
		return;
	}

	enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
	switch (type) {
	case PacketType::Reliable: flags = ENET_PACKET_FLAG_RELIABLE; break;
	case PacketType::Unreliable: flags = 0; break;
	}

	auto packet = enet_packet_create(data, size, flags);
	enet_peer_send(peer, channel, packet);
}

size_t Unet::ServiceEnet::ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel)
{
	if (channel >= m_channels.size()) {
		assert(false);
		return 0;
	}

	auto &queue = m_channels[channel];
	auto packet = queue.front();

	size_t actualSize = std::min(packet->dataLength, maxSize);
	memcpy(data, packet->data, actualSize);

	enet_packet_destroy(packet);
	queue.pop();

	return actualSize;
}

bool Unet::ServiceEnet::IsPacketAvailable(size_t* outPacketSize, uint8_t channel)
{
	if (m_host == nullptr) {
		return false;
	}

	if (channel >= m_channels.size()) {
		assert(false);
		return false;
	}

	auto &queue = m_channels[channel];
	if (queue.size() == 0) {
		return false;
	}

	auto packet = queue.front();
	if (outPacketSize != nullptr) {
		*outPacketSize = packet->dataLength;
	}

	return true;
}

ENetPeer* Unet::ServiceEnet::GetPeer(const ServiceID &id)
{
	for (auto peer : m_peers) {
		if (AddressToID(peer->address) == id) {
			return peer;
		}
	}
	return nullptr;
}

void Unet::ServiceEnet::Clear(int numChannels)
{
	for (auto &queue : m_channels) {
		while (queue.size() > 0) {
			enet_packet_destroy(queue.front());
			queue.pop();
		}
	}
	m_channels.clear();

	for (int i = 0; i < numChannels; i++) {
		m_channels.emplace_back(std::queue<ENetPacket*>());
	}
}