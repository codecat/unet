#pragma once

#include <Unet_common.h>
#include <Unet/Result.h>
#include <Unet/Service.h>

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
