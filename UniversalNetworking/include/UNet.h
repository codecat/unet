#pragma once

#include <UNet_common.h>

namespace Unet
{
	enum class Result
	{
		None,
		OK,
		Error,
	};
}

namespace Unet
{
	enum class ServiceType
	{
		None,
		Steam,
		Galaxy,
	};
}

namespace Unet
{
	struct ServiceEntryPoint
	{
		ServiceType Service;
		uint64_t ID;
	};
}

namespace Unet
{
	enum class LobbyPrivacy
	{
		Public,
		Private,
	};

	struct LobbyInfo
	{
		//TODO: Some unique ID so that it can be identified across services

		bool IsHosting = false;
		LobbyPrivacy Privacy = LobbyPrivacy::Public;
		int MaxPlayers = 0;

		std::string Name;
		std::vector<ServiceEntryPoint> EntryPoints;

		const ServiceEntryPoint* GetEntryPoint(ServiceType service) const;
	};
}

namespace Unet
{
	class Lobby
	{
	public:
		Lobby(const LobbyInfo &lobbyInfo);
		~Lobby();

		const LobbyInfo &GetInfo();
		bool IsConnected();

		void AddEntryPoint(ServiceEntryPoint entryPoint);
		void ServiceDisconnected(ServiceType service);

	private:
		LobbyInfo m_info;
	};
}

namespace Unet
{
	struct ResultObject
	{
		Result Result = Result::None;
	};
}

namespace Unet
{
	struct CreateLobbyResult : public ResultObject
	{
		Lobby* Lobby;
	};
}

namespace Unet
{
	struct LobbyListResult : public ResultObject
	{
		std::vector<LobbyInfo> Lobbies;
	};
}

namespace Unet
{
	struct LobbyJoinResult : public ResultObject
	{
		Lobby* Lobby;
	};
}

namespace Unet
{
	enum class LeaveReason
	{
		/// User intentially left the lobby (LeaveLobby was called)
		UserLeave,

		/// All service entry points have been disconnected
		Disconnected,
	};

	struct LobbyLeftResult : public ResultObject
	{
		LeaveReason Reason;
	};
}

namespace Unet
{
	class Service
	{
	public:
		virtual ServiceType GetType() = 0;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers) = 0;
		virtual void LeaveLobby() = 0;
	};
}

namespace Unet
{
	template<typename TResult>
	class MultiCallback
	{
	public:
		struct ServiceRequest
		{
			Result Result = Result::None;
			Service* Service = nullptr;
			TResult* Data = nullptr;
		};

	private:
		TResult m_result;
		std::vector<ServiceRequest*> m_requests;

	public:
		~MultiCallback()
		{
			Clear();
		}

		TResult &GetResult()
		{
			return m_result;
		}

		void Clear()
		{
			for (auto serviceRequest : m_requests) {
				delete serviceRequest;
			}
			m_requests.clear();
			m_result = TResult();
		}

		void Begin()
		{
			Clear();
		}

		bool Ready()
		{
			if (m_requests.size() == 0) {
				return false;
			}

			for (auto serviceRequest : m_requests) {
				if (serviceRequest->Result == Result::None) {
					return false;
				}
			}
			return true;
		}

		int NumOK()
		{
			int ret = 0;
			for (auto serviceRequest : m_requests) {
				if (serviceRequest->Result == Result::OK) {
					ret++;
				}
			}
			return ret;
		}

		int NumRequests()
		{
			return (int)m_requests.size();
		}

		ServiceRequest* AddServiceRequest(Service* service)
		{
			auto newServiceRequest = new ServiceRequest;
			newServiceRequest->Service = service;
			newServiceRequest->Data = &m_result;
			m_requests.emplace_back(newServiceRequest);
			return newServiceRequest;
		}
	};
}

namespace Unet
{
	class Callbacks
	{
	public:
		virtual void OnLobbyCreated(const CreateLobbyResult &result) {}
		virtual void OnLobbyList(const LobbyListResult &result) {}
		virtual void OnLobbyJoined(const LobbyJoinResult &result) {}
		virtual void OnLobbyLeft(const LobbyLeftResult &result) {}
		//TODO: OnLobbyConnectionFallback, specifying which p2p we've fallen back to
	};

	enum class ContextStatus
	{
		Idle,
		Connecting,
		Connected,
	};

	class Context
	{
	public:
		Context();
		virtual ~Context();

		ContextStatus GetStatus();

		/// Context takes ownership of the callbacks object
		void SetCallbacks(Callbacks* callbacks);
		void RunCallbacks();

		void SetPrimaryService(ServiceType service);
		void EnableService(ServiceType service);

		void CreateLobby(LobbyPrivacy privacy, int maxPlayers, const char* name = nullptr);
		void GetLobbyList();
		void JoinLobby(LobbyInfo &lobbyInfo);
		void LeaveLobby();
		Lobby* CurrentLobby();

	private:
		void OnLobbyCreated(const CreateLobbyResult &result);
		void OnLobbyList(const LobbyListResult &result);
		void OnLobbyJoined(const LobbyJoinResult &result);
		void OnLobbyLeft(const LobbyLeftResult &result);

	private:
		ContextStatus m_status;
		ServiceType m_primaryService;

		Callbacks* m_callbacks;

		Lobby* m_currentLobby;

		std::vector<Service*> m_services;

	public:
		MultiCallback<CreateLobbyResult> m_callbackCreateLobby;
		MultiCallback<LobbyListResult> m_callbackLobbyList;
		MultiCallback<LobbyJoinResult> m_callbackLobbyJoin;
		MultiCallback<LobbyLeftResult> m_callbackLobbyLeft;
	};
}

namespace Unet
{
	const char* GetVersion();

	ServiceType GetServiceTypeByName(const char* str);
	const char* GetServiceNameByType(ServiceType type);
}
