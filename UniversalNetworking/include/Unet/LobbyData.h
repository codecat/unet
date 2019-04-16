#pragma once

#include <Unet_common.h>

namespace Unet
{
	struct LobbyData
	{
		std::string Name;
		std::string Value;

		LobbyData();
		LobbyData(const std::string &name, const std::string &value);
	};

	class LobbyDataContainer
	{
	public:
		std::vector<LobbyData> m_data;

	public:
		virtual void SetData(const char* name, const std::string &value);
		virtual std::string GetData(const char* name) const;

		virtual json SerializeData() const;
		virtual void DeserializeData(const json &js);
	};
}
