#include <Unet_common.h>
#include <Unet/Context.h>
#include <Unet/Service.h>
#include <Unet/Services/ServiceSteam.h>
#include <Unet/Services/ServiceGalaxy.h>
#include <Unet/Utils.h>
#include <Unet/LobbyPacket.h>

#include <Unet/json.hpp>
using json = nlohmann::json;

Unet::Context::Context()
{
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

void Unet::Context::RunCallbacks()
{
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
		for (auto service : m_services) {
			size_t packetSize;
			while (service->IsPacketAvailable(&packetSize, 0)) {
				m_callbacks->OnLogDebug(strPrintF("[P2P] [%s] %d bytes", GetServiceNameByType(service->GetType()), (int)packetSize));

				std::vector<uint8_t> msg;
				msg.assign(packetSize, 0);

				ServiceID peer;
				service->ReadPacket(msg.data(), packetSize, &peer, 0);

				auto peerMember = m_currentLobby->GetMember(peer);

				json js = json::from_bson(msg);
				if (!js.is_object() || !js.contains("t")) {
					m_callbacks->OnLogError(strPrintF("[P2P] [%s] Message from 0x%08llX is not a valid bson object!", GetServiceNameByType(peer.Service), peer.ID));
					continue;
				}

				auto jsDump = js.dump();
				m_callbacks->OnLogDebug(strPrintF("[P2P] [%s] Message object: \"%s\"", GetServiceNameByType(peer.Service), jsDump.c_str()));

				auto type = (LobbyPacketType)(uint8_t)js["t"];

				if (type == LobbyPacketType::Handshake) {
					auto &lobbyInfo = m_currentLobby->GetInfo();
					if (!lobbyInfo.IsHosting) {
						continue;
					}

					xg::Guid guid(js["guid"].get<std::string>());
					auto &member = m_currentLobby->AddMemberService(guid, peer);

					if (member.Valid) {
						js = json::object();
						js["t"] = (uint8_t)LobbyPacketType::MemberNewService;
						js["guid"] = guid.str();
						js["service"] = (int)peer.Service;
						js["id"] = peer.ID;
						msg = json::to_bson(js);

						SendToAll(msg.data(), msg.size());
					}

				} else if (type == LobbyPacketType::Hello) {
					auto &lobbyInfo = m_currentLobby->GetInfo();
					if (!lobbyInfo.IsHosting) {
						continue;
					}

					auto member = m_currentLobby->GetMember(peer);
					if (member == nullptr) {
						m_callbacks->OnLogWarn(strPrintF("Received Hello packet from %s ID 0x%08llX before receiving any handshakes!",
							GetServiceNameByType(peer.Service), peer.ID
						));
						continue;
					}

					member->Name = js["name"].get<std::string>();
					member->UnetPrimaryService = peer.Service;
					member->Valid = true;

					js = json::object();
					js["t"] = (uint8_t)LobbyPacketType::LobbyInfo;
					js["data"] = json::object();
					for (auto &data : m_currentLobby->m_data) {
						js["data"][data.Name] = data.Value;
					}
					js["members"] = json::array();
					for (auto &member : m_currentLobby->m_members) {
						if (!member.Valid) {
							continue;
						}

						json jsMember;
						jsMember["guid"] = member.UnetGuid.str();
						jsMember["peer"] = member.UnetPeer;
						jsMember["name"] = member.Name;
						jsMember["ids"] = json::array();
						for (auto &id : member.IDs) {
							jsMember["ids"].emplace_back(json::array({ (int)id.Service, id.ID }));
						}
						jsMember["data"] = json::object();
						for (auto &memberData : member.Data) {
							jsMember["data"][memberData.Name] = memberData.Value;
						}
						js["members"].emplace_back(jsMember);
					}
					msg = json::to_bson(js);

					SendTo(*member, msg.data(), msg.size());

					m_callbacks->OnLobbyPlayerJoined(*member);

					//TODO: Send member info to all other clients
					//TODO: Call OnLobbyMemberJoined callback

				} else if (type == LobbyPacketType::LobbyInfo) {
					auto &lobbyInfo = m_currentLobby->GetInfo();
					if (lobbyInfo.IsHosting) {
						continue;
					}

					for (auto &pair : js["data"].items()) {
						for (auto &data : m_currentLobby->m_data) {
							if (data.Name == pair.key()) {
								data.Value = pair.value().get<std::string>();
								break;
							}
						}
					}

					for (auto &member : js["members"]) {
						xg::Guid guid(member["guid"].get<std::string>());

						for (auto &memberId : member["ids"]) {
							auto service = (ServiceType)memberId[0].get<int>();
							auto id = memberId[1].get<uint64_t>();

							m_currentLobby->AddMemberService(guid, ServiceID(service, id));
						}

						auto lobbyMember = m_currentLobby->GetMember(guid);
						assert(lobbyMember != nullptr); // If this fails, there's no service IDs available for this member

						lobbyMember->UnetPeer = member["peer"].get<int>();
						lobbyMember->Name = member["name"].get<std::string>();

						for (auto &pair : member["data"].items()) {
							for (auto &data : lobbyMember->Data) {
								if (data.Name == pair.key()) {
									data.Value = pair.value().get<std::string>();
									break;
								}
							}
						}
					}

					for (auto &member : m_currentLobby->m_members) {
						if (member.UnetGuid == m_localGuid) {
							m_localPeer = member.UnetPeer;
						}
					}

					m_status = ContextStatus::Connected;

					LobbyJoinResult result;
					result.Code = Result::OK;
					//TODO: Set result.JoinGuid? It's not super important..
					result.JoinedLobby = m_currentLobby;
					m_callbacks->OnLobbyJoined(result);

				} else if (type == LobbyPacketType::MemberNewService) {
					auto &lobbyInfo = m_currentLobby->GetInfo();
					if (lobbyInfo.IsHosting) {
						continue;
					}

					xg::Guid guid(js["guid"].get<std::string>());

					ServiceID id;
					id.Service = (ServiceType)js["service"].get<int>();
					id.ID = js["id"].get<uint64_t>();

					m_currentLobby->AddMemberService(guid, id);

				} else {
					m_callbacks->OnLogWarn(strPrintF("P2P packet type was not recognized: %d", (int)type));
				}
			}
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

void Unet::Context::CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name)
{
	m_status = ContextStatus::Connecting;

	m_callbackCreateLobby.Begin();

	m_localGuid = xg::newGuid();
	m_localPeer = 0;

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

		int maxPlayers = service->GetLobbyMaxPlayers(*entry);
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
	ServiceType firstService = ServiceType::None;
	std::string ret;

	for (size_t i = 0; i < lobbyInfo.EntryPoints.size(); i++) {
		auto &entry = lobbyInfo.EntryPoints[i];

		auto service = GetService(entry.Service);
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

		int numData = service->GetLobbyDataCount(entry);
		for (int i = 0; i < numData; i++) {
			auto data = service->GetLobbyData(entry, i);

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

void Unet::Context::SetPersonaName(const std::string &str)
{
	m_personaName = str;
}

const std::string &Unet::Context::GetPersonaName()
{
	return m_personaName;
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

void Unet::Context::SendTo(LobbyMember &member, uint8_t* data, size_t size)
{
	//TODO: Implement relaying through host if this is a client-to-client message where there's no compatible connection (eg. Steam to Galaxy communication)

	auto id = member.GetPrimaryServiceID();
	auto service = GetService(id.Service);

	assert(service != nullptr);
	if (service == nullptr) {
		return;
	}

	// Sending a message to yourself isn't very useful.
	assert(member.UnetPeer != m_localPeer);

	service->SendPacket(id, data, size, PacketType::Reliable, 0);
}

void Unet::Context::SendToAll(uint8_t* data, size_t size)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (member.UnetPeer != m_localPeer) {
			SendTo(member, data, size);
		}
	}
}

void Unet::Context::SendToAllExcept(LobbyMember &exceptMember, uint8_t* data, size_t size)
{
	assert(m_currentLobby != nullptr);
	if (m_currentLobby == nullptr) {
		return;
	}

	for (auto &member : m_currentLobby->m_members) {
		if (member.UnetPeer != m_localPeer && member.UnetPeer != exceptMember.UnetPeer) {
			SendTo(member, data, size);
		}
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
		newMember.Name = PrimaryService()->GetUserName();
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

		m_callbacks->OnLobbyJoined(result);
		return;

	} else {
		m_currentLobby = result.JoinedLobby;
	}

	auto lobbyId = m_currentLobby->GetPrimaryEntryPoint();
	assert(lobbyId.IsValid());

	auto service = GetService(lobbyId.Service);
	assert(service != nullptr);

	auto lobbyHost = service->GetLobbyHost(lobbyId);
	assert(lobbyHost.IsValid());

	json js;
	js["t"] = (uint8_t)LobbyPacketType::Hello;
	js["name"] = m_personaName;
	std::vector<uint8_t> msg = json::to_bson(js);

	service->SendPacket(lobbyHost, msg.data(), msg.size(), PacketType::Reliable, 0);

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

	if (m_callbacks != nullptr) {
		m_callbacks->OnLobbyLeft(result);
	}
}
