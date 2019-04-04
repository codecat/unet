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

	class LobbyCreatedListener : public GalaxyListener, public galaxy::api::ILobbyCreatedListener
	{
	public:
		LobbyCreatedListener(ServiceGalaxy* self) { m_self = self; }
		virtual void OnLobbyCreated(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyCreateResult result) override;
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

	class LobbyJoinListener : public GalaxyListener, public galaxy::api::ILobbyEnteredListener
	{
	public:
		LobbyJoinListener(ServiceGalaxy* self) { m_self = self; }
		virtual void OnLobbyEntered(const galaxy::api::GalaxyID& lobbyID, galaxy::api::LobbyEnterResult result) override;
	};

	class LobbyLeftListener : public GalaxyListener, public galaxy::api::ILobbyLeftListener
	{
	public:
		LobbyLeftListener(ServiceGalaxy* self) { m_self = self; }
		virtual void OnLobbyLeft(const galaxy::api::GalaxyID& lobbyID, LobbyLeaveReason leaveReason) override;
	};

	class ServiceGalaxy : public Service
	{
		friend class LobbyLeftListener;

	private:
		LobbyCreatedListener m_lobbyCreatedListener;
		LobbyListListener m_lobbyListListener;
		LobbyJoinListener m_lobbyJoinListener;
		LobbyLeftListener m_lobbyLeftListener;

	public:
		MultiCallback<CreateLobbyResult>::ServiceRequest* m_requestLobbyCreated = nullptr;
		MultiCallback<LobbyListResult>::ServiceRequest* m_requestLobbyList = nullptr;
		MultiCallback<LobbyJoinResult>::ServiceRequest* m_requestLobbyJoin = nullptr;
		MultiCallback<LobbyLeftResult>::ServiceRequest* m_requestLobbyLeft = nullptr;

	public:
		ServiceGalaxy(Context* ctx);
		virtual ~ServiceGalaxy();

		virtual ServiceType GetType() override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) override;
		virtual void GetLobbyList() override;
		virtual void JoinLobby(const LobbyInfo &lobbyInfo) override;
		virtual void LeaveLobby() override;

		virtual int GetLobbyMaxPlayers(uint64_t lobbyId) override;

		virtual std::string GetLobbyData(uint64_t lobbyId, const char* name) override;
		virtual int GetLobbyDataCount(uint64_t lobbyId) override;
		virtual LobbyData GetLobbyData(uint64_t lobbyId, int index) override;

		virtual void SetLobbyData(const LobbyInfo &lobbyInfo, const char* name, const char* value) override;
	};
}
