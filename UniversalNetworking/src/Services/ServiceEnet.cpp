#include <Unet_common.h>
#include <Unet/Services/ServiceEnet.h>
#include <Unet/Utils.h>

#define UNET_PORT 4450

static Unet::ServiceID AddressToID(const ENetAddress &addr)
{
	return Unet::ServiceID(Unet::ServiceType::Enet, *(uint64_t*)&addr);
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
			m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Enet] Connect event: %08X", ev.peer->address.host));

			if (m_requestLobbyJoin != nullptr) {
				m_requestLobbyJoin->Code = Result::OK;
				m_requestLobbyJoin->Data->JoinedLobby->AddEntryPoint(AddressToID(ev.peer->address));
			}
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

	m_host = enet_host_create(&addr, maxPlayers, maxChannels, 0, 0);

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

	m_host = enet_host_create(nullptr, 1, maxChannels, 0, 0);
	ENetPeer* peer = enet_host_connect(m_host, &addr, maxChannels, 0);
}

void Unet::ServiceEnet::LeaveLobby()
{
	auto req = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);
	req->Data->Reason = LeaveReason::UserLeave;
	req->Code = Result::OK;
}

int Unet::ServiceEnet::GetLobbyMaxPlayers(const ServiceID &lobbyId)
{
	//TODO
	return 0;
}

Unet::ServiceID Unet::ServiceEnet::GetLobbyHost(const ServiceID &lobbyId)
{
	//TODO
	return ServiceID();
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
	//TODO
}

size_t Unet::ServiceEnet::ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel)
{
	//TODO
	return 0;
}

bool Unet::ServiceEnet::IsPacketAvailable(size_t* outPacketSize, uint8_t channel)
{
	//TODO
	return false;
}
