#pragma once

#include <Unet_common.h>
#include <Unet/Service.h>
#include <Unet/Context.h>

#include <galaxy/GalaxyApi.h>

namespace Unet
{
	class GalaxyListener
	{
	protected:
		class ServiceGalaxy* m_self;
	};

	class LobbyListListener : public GalaxyListener, public galaxy::api::ILobbyListListener, public galaxy::api::ILobbyDataRetrieveListener
	{
	private:
		std::vector<galaxy::api::GalaxyID> m_dataFetch;

	public:
		LobbyListListener(ServiceGalaxy* self) { m_self = self; }
		virtual void OnLobbyList(uint32_t lobbyCount, galaxy::api::LobbyListResult result) override;
		void LobbyDataUpdated();
		virtual void OnLobbyDataRetrieveSuccess(const galaxy::api::GalaxyID& lobbyID) override;
		virtual void OnLobbyDataRetrieveFailure(const galaxy::api::GalaxyID& lobbyID, FailureReason failureReason) override;
	};

	class ServiceGalaxy : public Service,
		galaxy::api::ILobbyCreatedListener,
		galaxy::api::ILobbyEnteredListener,
		galaxy::api::ILobbyLeftListener,
		galaxy::api::ILobbyMemberStateListener
	{
	private:
		LobbyListListener m_lobbyListListener;

	public:
		MultiCallback<CreateLobbyResult>::ServiceRequest* m_requestLobbyCreated = nullptr;
		MultiCallback<LobbyListResult>::ServiceRequest* m_requestLobbyList = nullptr;
		MultiCallback<LobbyJoinResult>::ServiceRequest* m_requestLobbyJoin = nullptr;
		MultiCallback<LobbyLeftResult>::ServiceRequest* m_requestLobbyLeft = nullptr;

	public:
		ServiceGalaxy(Context* ctx);
		virtual ~ServiceGalaxy();

		virtual ServiceType GetType() override;

		virtual ServiceID GetUserID() override;
		virtual std::string GetUserName() override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) override;
		virtual void GetLobbyList() override;
		virtual void JoinLobby(const ServiceID &id) override;
		virtual void LeaveLobby() override;

		virtual int GetLobbyMaxPlayers(const ServiceID &lobbyId) override;
		virtual ServiceID GetLobbyHost(const ServiceID &lobbyId) override;

		virtual std::string GetLobbyData(const ServiceID &lobbyId, const char* name) override;
		virtual int GetLobbyDataCount(const ServiceID &lobbyId) override;
		virtual LobbyData GetLobbyData(const ServiceID &lobbyId, int index) override;

		virtual void SetLobbyData(const ServiceID &lobbyId, const char* name, const char* value) override;

		virtual void SendPacket(const ServiceID &peerId, const void* data, size_t size, PacketType type, uint8_t channel) override;
		virtual size_t ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel) override;
		virtual bool IsPacketAvailable(size_t* outPacketSize, uint8_t channel) override;

	private:
		virtual void OnLobbyCreated(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyCreateResult result) override;
		virtual void OnLobbyEntered(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyEnterResult result) override;
		virtual void OnLobbyLeft(const galaxy::api::GalaxyID& lobbyID, LobbyLeaveReason leaveReason) override;
		virtual void OnLobbyMemberStateChanged(const galaxy::api::GalaxyID& lobbyID, const galaxy::api::GalaxyID& memberID, galaxy::api::LobbyMemberStateChange memberStateChange) override;
	};
}
