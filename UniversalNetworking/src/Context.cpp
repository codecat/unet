#include <Unet_common.h>
#include <Unet/Context.h>
#include <Unet/Service.h>
#include <Unet/Services/ServiceSteam.h>
#include <Unet/Services/ServiceGalaxy.h>
#include <Unet/Services/ServiceEnet.h>
#include <Unet/LobbyPacket.h>

#define XXH_INLINE_ALL
#include <Unet/xxhash.h>

Unet::Context::Context(int numChannels)
	: m_reassembly(this)
{
	m_numChannels = numChannels;
	m_queuedMessages.assign(numChannels, std::queue<NetworkMessage*>());

	m_status = ContextStatus::Idle;
	m_primaryService = ServiceType::None;

	m_callbacks = nullptr;

	m_currentLobby = nullptr;
	m_localPeer = -1;
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

	for (auto service : m_services) {
		delete service;
	}

	for (auto &channel : m_queuedMessages) {
		while (channel.size() > 0) {
			delete channel.front();
			channel.pop();
		}
	}
}

Unet::ContextStatus Unet::Context::GetStatus()
{
	return m_status;
}

void Unet::Context::SetCallbacks(Callbacks* callbacks)
{
	m_callbacks = callbacks;
}

Unet::Callbacks* Unet::Context::GetCallbacks()
{
	return m_callbacks;
}

template<typename TResult, typename TFunc>
static void CheckCallback(Unet::Context* ctx, Unet::MultiCallback<TResult> &callback, TFunc func)
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

void Unet::Context::RunCallbacks()
{
	for (auto service : m_services) {
		service->RunCallbacks();
	}

	CheckCallback(this, m_callbackCreateLobby, &Context::OnLobbyCreated);
	CheckCallback(this, m_callbackLobbyList, &Context::OnLobbyList);
	CheckCallback(this, m_callbackLobbyJoin, &Context::OnLobbyJoined);
	CheckCallback(this, m_callbackLobbyLeft, &Context::OnLobbyLeft);

	if (m_status == ContextStatus::Connected && m_currentLobby != nullptr) {
		if (!m_currentLobby->IsConnected()) {
			m_callbacks->OnLogError("Connection to lobby was lost");

			LobbyLeftResult result;
			result.Code = Result::OK;
			result.Reason = LeaveReason::Disconnected;
			OnLobbyLeft(result);
		}
	}

	if (m_currentLobby != nullptr) {
		std::vector<uint8_t> msg;

		for (auto service : m_services) {
			size_t packetSizeLimit = service->ReliablePacketLimit();

			size_t packetSize;

			if (packetSizeLimit > 0) {
				// Re-assembly for internal lobby message channel
				while (service->IsPacketAvailable(&packetSize, 0)) {
					msg.resize(packetSize);

					ServiceID peer;
					service->ReadPacket(msg.data(), packetSize, &peer, 0);
					uint8_t * msgData = msg.data();

					m_reassembly.HandleMessage(peer, -1, msgData, packetSize);
				}

				// Re-assembly for general purpose channels
				for (int channel = 0; channel < m_numChannels; channel++) {
					while (service->IsPacketAvailable(&packetSize, 2 + channel)) {
						msg.resize(packetSize);

						ServiceID peer;
						service->ReadPacket(msg.data(), packetSize, &peer, 2 + channel);
						uint8_t * msgData = msg.data();

						m_reassembly.HandleMessage(peer, channel, msgData, packetSize);
					}
				}
			}

			// Relay packet channel
			while (service->IsPacketAvailable(&packetSize, 1)) {
				msg.resize(packetSize);

				ServiceID peer;
				service->ReadPacket(msg.data(), packetSize, &peer, 1);
				uint8_t* msgData = msg.data();

				auto peerMember = m_currentLobby->GetMember(peer);

				if (m_currentLobby->m_info.IsHosting) {
					// We have to relay a packet to some client
					uint8_t peerRecipient = *(msgData++);
					uint8_t channel = *(msgData++);
					PacketType type = (PacketType)*(msgData++);
					packetSize -= 3;

					auto recipientMember = m_currentLobby->GetMember((int)peerRecipient);
					if (recipientMember == nullptr) {
						m_callbacks->OnLogError(strPrintF("Tried relaying packet of %d bytes to unknown peer %d!", (int)packetSize, (int)peerRecipient));
						continue;
					}

					std::vector<uint8_t> relayMsg;
					relayMsg.assign(packetSize + 2, 0);

					relayMsg[0] = (uint8_t)peerMember->UnetPeer;
					relayMsg[1] = channel;
					memcpy(relayMsg.data() + 2, msgData, packetSize);

					auto id = recipientMember->GetDataServiceID();
					assert(id.IsValid());
					if (!id.IsValid()) {
						continue;
					}

					auto recipientService = GetService(id.Service);
					assert(recipientService != nullptr);
					if (recipientService == nullptr) {
						continue;
					}

					recipientService->SendPacket(id, relayMsg.data(), relayMsg.size(), type, 1);

				} else {
					// We received a relayed packet from some client
					uint8_t peerSender = *(msgData++);
					uint8_t channel = *(msgData++);
					packetSize -= 2;

					if (channel >= (uint8_t)m_queuedMessages.size()) {
						m_callbacks->OnLogError(strPrintF("Invalid channel index in relay packet: %d", (int)channel));
						continue;
					}

					auto memberSender = m_currentLobby->GetMember(peerSender);
					assert(memberSender != nullptr);
					if (memberSender == nullptr) {
						m_callbacks->OnLogError(strPrintF("Received a relay packet from unknown peer %d", (int)peerSender));
						continue;
					}

					if (packetSizeLimit > 0) {
						m_reassembly.HandleMessage(memberSender->GetPrimaryServiceID(), (int)channel, msgData, packetSize);
					} else {
						auto newMessage = new NetworkMessage(msgData, packetSize);
						newMessage->m_channel = (int)channel;
						newMessage->m_peer = memberSender->GetPrimaryServiceID();
						m_queuedMessages[channel].push(newMessage);
					}
				}
			}

			if (packetSizeLimit == 0) {
				while (service->IsPacketAvailable(&packetSize, 0)) {
					std::vector<uint8_t> msg;
					msg.assign(packetSize, 0);

					ServiceID peer;
					service->ReadPacket(msg.data(), packetSize, &peer, 0);

					m_currentLobby->HandleMessage(peer, msg.data(), packetSize);
				}
			}
		}
	}

	// Pop any fragmented messages into the message queue
	while (auto msg = m_reassembly.PopReady()) {
		if (msg->m_channel == -1) {
			m_currentLobby->HandleMessage(msg->m_peer, msg->m_data, msg->m_size);
			delete msg;
		} else {
			m_queuedMessages[msg->m_channel].push(msg);
		}
	}
}

void Unet::Context::SetPrimaryService(ServiceType service)
{
	auto s = GetService(service);
	if (s == nullptr) {
		m_callbacks->OnLogError(strPrintF("Service %s is not enabled, so it can't be set as the primary service!", GetServiceNameByType(service)));
		return;
	}

	m_primaryService = service;

	m_personaName = s->GetUserName();
}

void Unet::Context::EnableService(ServiceType service)
{
	Service* newService = nullptr;
	switch (service) {
	case ServiceType::Steam: newService = new ServiceSteam(this); break;
	case ServiceType::Galaxy: newService = new ServiceGalaxy(this); break;
	case ServiceType::Enet: newService = new ServiceEnet(this); break;
	default: assert(false);
	}

	if (newService == nullptr) {
		m_callbacks->OnLogError(strPrintF("Couldn't make new \"%s\" service!", GetServiceNameByType(service)));
		return;
	}

	m_services.emplace_back(newService);

	if (m_primaryService == ServiceType::None) {
		SetPrimaryService(service);
	}
}

int Unet::Context::ServiceCount()
{
	return (int)m_services.size();
}

void Unet::Context::SimulateServiceOutage(ServiceType type)
{
	if (m_currentLobby == nullptr) {
		return;
	}

	auto service = GetService(type);
	if (service != nullptr) {
		service->SimulateOutage();
		m_currentLobby->ServiceDisconnected(type);
	}
}

void Unet::Context::CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name)
{
	m_status = ContextStatus::Connecting;

	m_callbackCreateLobby.Begin();

	m_localGuid = xg::newGuid();
	m_localPeer = 0;

	for (auto &channel : m_queuedMessages) {
		while (channel.size() > 0) {
			delete channel.front();
			channel.pop();
		}
	}

	auto &result = m_callbackCreateLobby.GetResult();
	LobbyInfo newLobbyInfo;
	newLobbyInfo.IsHosting = true;
	newLobbyInfo.Privacy = privacy;
	newLobbyInfo.MaxPlayers = maxPlayers;
	newLobbyInfo.UnetGuid = m_localGuid;
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
	m_callbackLobbyList.GetResult().Ctx = this;

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

	m_localGuid = xg::newGuid();
	m_localPeer = -1;

	for (auto &channel : m_queuedMessages) {
		while (channel.size() > 0) {
			delete channel.front();
			channel.pop();
		}
	}

	auto &result = m_callbackLobbyJoin.GetResult();
	result.JoinGuid = m_localGuid;
	result.JoinedLobby = new Lobby(this, lobbyInfo);

	for (auto service : m_services) {
		auto entry = lobbyInfo.GetEntryPoint(service->GetType());
		if (entry != nullptr) {
			service->JoinLobby(*entry);
		}
	}
}

void Unet::Context::JoinLobby(const ServiceID &id)
{
	if (m_status != ContextStatus::Idle) {
		m_callbacks->OnLogError("Can't join new lobby while still in a lobby!");
		return;
	}

	auto service = GetService(id.Service);
	if (service == nullptr) {
		m_callbacks->OnLogError(strPrintF("Can't join lobby with service ID for %d, service is not enabled!", GetServiceNameByType(id.Service)));
		return;
	}

	m_status = ContextStatus::Connecting;

	m_callbackLobbyJoin.Begin();

	m_localGuid = xg::newGuid();
	m_localPeer = -1;

	auto &result = m_callbackLobbyJoin.GetResult();
	result.JoinGuid = m_localGuid;

	LobbyInfo newLobbyInfo;
	newLobbyInfo.EntryPoints.emplace_back(id);
	result.JoinedLobby = new Lobby(this, newLobbyInfo);

	service->JoinLobby(id);
}

void Unet::Context::LeaveLobby(LeaveReason reason)
{
	if (m_status == ContextStatus::Connected) {
		m_callbackLobbyLeft.Begin();
		m_callbackLobbyLeft.GetResult().Reason = reason;

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

int Unet::Context::GetLocalPeer()
{
	return m_localPeer;
}

void Unet::Context::SetPersonaName(const std::string &str)
{
	m_personaName = str;
}

const std::string &Unet::Context::GetPersonaName()
{
	return m_personaName;
}

bool Unet::Context::IsMessageAvailable(int channel)
{
	if (channel < 0) {
		return false;
	}

	if (channel < (int)m_queuedMessages.size()) {
		auto &queuedChannel = m_queuedMessages[channel];
		if (queuedChannel.size() > 0) {
			return true;
		}
	}

	for (auto service : m_services) {
		// For services that have a reliable packet limit, we only allow queued messages
		if (service->ReliablePacketLimit() > 0) {
			continue;
		}

		if (service->IsPacketAvailable(nullptr, 2 + channel)) {
			return true;
		}
	}

	return false;
}

std::unique_ptr<Unet::NetworkMessage> Unet::Context::ReadMessage(int channel)
{
	if (channel < 0) {
		return nullptr;
	}

	if (channel < (int)m_queuedMessages.size()) {
		auto &queuedChannel = m_queuedMessages[channel];
		if (queuedChannel.size() > 0) {
			std::unique_ptr<NetworkMessage> ret(queuedChannel.front());
			queuedChannel.pop();
			return ret;
		}
	}

	for (auto service : m_services) {
		// For services that have a reliable packet limit, we only allow queued messages
		if (service->ReliablePacketLimit() > 0) {
			continue;
		}

		size_t packetSize;
		if (service->IsPacketAvailable(&packetSize, 2 + channel)) {
			std::unique_ptr<NetworkMessage> newMessage(new NetworkMessage(packetSize));
			newMessage->m_channel = channel;
			newMessage->m_size = service->ReadPacket(newMessage->m_data, packetSize, &newMessage->m_peer, 2 + channel);
			return newMessage;
		}
	}

	return nullptr;
}

void Unet::Context::SendTo_Impl(LobbyMember &member, uint8_t* data, size_t size, PacketType type, uint8_t channel)
{
	// Sending a message to yourself isn't very useful.
	assert(member.UnetPeer != m_localPeer);
	if (member.UnetPeer == m_localPeer) {
		return;
	}

	auto id = member.GetDataServiceID();
	auto service = GetService(id.Service);

	if (!id.IsValid() || service == nullptr) {
		// Data service to peer is not available, so we have to relay it through the host
		auto hostMember = m_currentLobby->GetHostMember();
		assert(hostMember != nullptr);

		auto idHost = hostMember->GetDataServiceID();
		assert(idHost.IsValid());
		auto serviceHost = GetService(idHost.Service);
		assert(serviceHost != nullptr);

		std::vector<uint8_t> msg;
		msg.assign(size + 3, 0);

		msg[0] = (uint8_t)member.UnetPeer;
		msg[1] = channel;
		msg[2] = (uint8_t)type;
		memcpy(msg.data() + 3, data, size);

		//TODO: Send this with the actual type instead? (That would make it twice as unreliable if it's sent unreliably!)
		serviceHost->SendPacket(idHost, msg.data(), msg.size(), PacketType::Reliable, 1);
		return;
	}

	service->SendPacket(id, data, size, type, channel + 2);
}

void Unet::Context::SendTo(LobbyMember &member, uint8_t* data, size_t size, PacketType type, uint8_t channel)
{
	auto id = member.GetDataServiceID();
	auto service = GetService(id.Service);

	size_t sizeLimit = service->ReliablePacketLimit();
	if (sizeLimit == 0) {
		SendTo_Impl(member, data, size, PacketType::Reliable, channel);
		return;
	}

	m_reassembly.SplitMessage(data, size, type, sizeLimit, [this, &member, type, channel](uint8_t* data, size_t size) {
		SendTo_Impl(member, data, size, type, channel);
	});
}

void Unet::Context::SendToAll(uint8_t* data, size_t size, PacketType type, uint8_t channel)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (!member.Valid) {
			continue;
		}

		if (member.UnetPeer == m_localPeer) {
			continue;
		}

		SendTo(member, data, size, type, channel);
	}
}

void Unet::Context::SendToAllExcept(LobbyMember &exceptMember, uint8_t* data, size_t size, PacketType type, uint8_t channel)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (!member.Valid) {
			continue;
		}

		if (member.UnetPeer == m_localPeer) {
			continue;
		}

		if (member.UnetPeer == exceptMember.UnetPeer) {
			continue;
		}

		SendTo(member, data, size, type, channel);
	}
}

void Unet::Context::SendToHost(uint8_t* data, size_t size, PacketType type, uint8_t channel)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	auto hostMember = m_currentLobby->GetHostMember();
	assert(hostMember != nullptr);
	if (hostMember == nullptr) {
		return;
	}

	SendTo(*hostMember, data, size, type, channel);
}

void Unet::Context::Kick(LobbyMember &member)
{
	if (!m_currentLobby->m_info.IsHosting) {
		m_callbacks->OnLogError("Can't kick members when not hosting!");
		return;
	}

	json js;
	js["t"] = (uint8_t)LobbyPacketType::MemberKick;
	InternalSendTo(member, js);
}

Unet::Service* Unet::Context::PrimaryService()
{
	auto ret = GetService(m_primaryService);
	assert(ret != nullptr);
	return ret;
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

void Unet::Context::InternalSendTo(LobbyMember &member, const json &js)
{
	// Sending a message to yourself isn't very useful.
	assert(member.UnetPeer != m_localPeer);

	auto id = member.GetDataServiceID();
	assert(id.IsValid());
	if (!id.IsValid()) {
		return;
	}

	InternalSendTo(id, js);
}

void Unet::Context::InternalSendTo(const ServiceID &id, const json &js)
{
	//TODO: Implement relaying through host if this is a client-to-client message where there's no compatible connection (eg. Steam to Galaxy communication)
	//NOTE: The above is not important yet for internal messages, as all internal messages are sent between client & server, not client & client

	auto service = GetService(id.Service);
	assert(service != nullptr);
	if (service == nullptr) {
		return;
	}

	auto msg = JsonPack(js);

	size_t sizeLimit = service->ReliablePacketLimit();
	if (sizeLimit == 0) {
		service->SendPacket(id, msg.data(), msg.size(), PacketType::Reliable, 0);
		return;
	}

	m_reassembly.SplitMessage(msg.data(), msg.size(), PacketType::Reliable, sizeLimit, [service, id](uint8_t* data, size_t size) {
		service->SendPacket(id, data, size, PacketType::Reliable, 0);
	});
}

void Unet::Context::InternalSendToAll(const json &js)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (member.UnetPeer != m_localPeer) {
			InternalSendTo(member, js);
		}
	}
}

void Unet::Context::InternalSendToAllExcept(LobbyMember &exceptMember, const json &js)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (member.UnetPeer != m_localPeer && member.UnetPeer != exceptMember.UnetPeer) {
			InternalSendTo(member, js);
		}
	}
}

void Unet::Context::InternalSendToHost(const json &js)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	auto hostMember = m_currentLobby->GetHostMember();
	if (hostMember != nullptr) {
		InternalSendTo(*hostMember, js);

	} else {
		auto primaryEntryPoint = m_currentLobby->GetPrimaryEntryPoint();
		assert(primaryEntryPoint.IsValid());

		auto service = GetService(primaryEntryPoint.Service);
		assert(service != nullptr);

		auto hostId = service->GetLobbyHost(primaryEntryPoint);
		InternalSendTo(hostId, js);
	}
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

		LobbyMember newMember(this);
		newMember.UnetGuid = xg::newGuid();
		newMember.UnetPeer = 0;
		newMember.UnetPrimaryService = m_primaryService;
		newMember.Name = m_personaName;
		for (auto service : m_services) {
			newMember.IDs.emplace_back(service->GetUserID());
		}
		m_currentLobby->m_members.emplace_back(newMember);
	}

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyCreated(result);
	}
}

void Unet::Context::OnLobbyList(const LobbyListResult &result)
{
	LobbyListResult newResult(result);

	for (auto &lobbyInfo : newResult.Lobbies) {
		lobbyInfo.MaxPlayers = result.GetLobbyMaxPlayers(lobbyInfo);
		lobbyInfo.Name = result.GetLobbyData(lobbyInfo, "unet-name");
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

		m_callbacks->OnLobbyJoined(result);
		return;

	} else {
		m_currentLobby = result.JoinedLobby;
	}

	json js;
	js["t"] = (uint8_t)LobbyPacketType::Hello;
	js["name"] = m_personaName;
	InternalSendToHost(js);

	m_callbacks->OnLogDebug("Hello sent");
}

void Unet::Context::OnLobbyLeft(const LobbyLeftResult &result)
{
	m_status = ContextStatus::Idle;
	m_localPeer = -1;

	if (m_currentLobby != nullptr) {
		delete m_currentLobby;
		m_currentLobby = nullptr;
	}

	for (auto &channel : m_queuedMessages) {
		while (channel.size() > 0) {
			delete channel.front();
			channel.pop();
		}
	}

	m_callbacks->OnLobbyLeft(result);
}

void Unet::Context::OnLobbyPlayerLeft(const LobbyMember &member)
{
	if (!member.Valid) {
		return;
	}

	if (m_currentLobby->m_info.IsHosting) {
		json js;
		js["t"] = (uint8_t)LobbyPacketType::MemberLeft;
		js["guid"] = member.UnetGuid.str();
		InternalSendToAll(js);
	}

	m_callbacks->OnLobbyPlayerLeft(member);
}
