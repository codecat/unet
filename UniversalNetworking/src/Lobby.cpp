#include <Unet_common.h>
#include <Unet/Lobby.h>

Unet::Lobby::Lobby(const LobbyInfo &lobbyInfo)
{
	m_info = lobbyInfo;
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

void Unet::Lobby::AddEntryPoint(ServiceEntryPoint entryPoint)
{
	m_info.EntryPoints.emplace_back(entryPoint);
}

void Unet::Lobby::ServiceDisconnected(ServiceType service)
{
	auto it = std::find_if(m_info.EntryPoints.begin(), m_info.EntryPoints.end(), [service](const ServiceEntryPoint &entryPoint) {
		return entryPoint.Service == service;
	});

	if (it == m_info.EntryPoints.end()) {
		return;
	}

	m_info.EntryPoints.erase(it);

	if (IsConnected()) {
		printf("Lost connection to entry point %s (%d points still open)\n", GetServiceNameByType(service), (int)m_info.EntryPoints.size());
	} else {
		printf("Lost connection to all entry points\n");
	}
}
