#pragma once

#include <Unet_common.h>

namespace Unet
{
	struct LobbyData
	{
		std::string Name;
		std::string Value;
	};

	class LobbyDataContainer
	{
	protected:
		std::vector<LobbyData> m_data;

	public:
		virtual void SetData(const char* name, const std::string &value);
		virtual std::string GetData(const char* name);
		virtual const std::vector<LobbyData> &GetAllData();
	};
}
