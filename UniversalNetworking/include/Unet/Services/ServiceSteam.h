#pragma once

#include <Unet_common.h>
#include <Unet/Service.h>
#include <Unet/Context.h>

#include <steam/steam_api.h>

namespace Unet
{
	class ServiceSteam : public Service
	{
	private:
		std::vector<uint64_t> m_listDataFetch;

		MultiCallback<CreateLobbyResult>::ServiceRequest* m_requestLobbyCreated = nullptr;
		CCallResult<ServiceSteam, LobbyCreated_t> m_callLobbyCreated;

		MultiCallback<LobbyListResult>::ServiceRequest* m_requestLobbyList = nullptr;
		CCallResult<ServiceSteam, LobbyMatchList_t> m_callLobbyList;

		MultiCallback<LobbyJoinResult>::ServiceRequest* m_requestLobbyJoin = nullptr;
		CCallResult<ServiceSteam, LobbyEnter_t> m_callLobbyJoin;

		CCallback<ServiceSteam, LobbyDataUpdate_t> m_callLobbyDataUpdate;
		CCallback<ServiceSteam, LobbyKicked_t> m_callLobbyKicked;

	public:
		ServiceSteam(Context* ctx);
		virtual ~ServiceSteam();

		virtual ServiceType GetType() override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) override;
		virtual void GetLobbyList() override;
		virtual void JoinLobby(uint64_t lobbyId) override;
		virtual void LeaveLobby() override;

		virtual int GetLobbyMaxPlayers(uint64_t lobbyId) override;

		virtual std::string GetLobbyData(uint64_t lobbyId, const char* name) override;
		virtual int GetLobbyDataCount(uint64_t lobbyId) override;
		virtual LobbyData GetLobbyData(uint64_t lobbyId, int index) override;

		virtual void SetLobbyData(uint64_t lobbyId, const char* name, const char* value) override;

	private:
		void OnLobbyCreated(LobbyCreated_t* result, bool bIOFailure);
		void OnLobbyList(LobbyMatchList_t* result, bool bIOFailure);
		void OnLobbyJoin(LobbyEnter_t* result, bool bIOFailure);

		void LobbyListDataUpdated();
		void OnLobbyDataUpdate(LobbyDataUpdate_t* result);
		void OnLobbyKicked(LobbyKicked_t* result);
	};
}
