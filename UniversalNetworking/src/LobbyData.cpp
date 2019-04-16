#include <Unet_common.h>
#include <Unet/LobbyData.h>
#include <Unet/ServiceType.h>

using json = nlohmann::json;

Unet::LobbyData::LobbyData()
{
}

Unet::LobbyData::LobbyData(const std::string &name, const std::string &value)
{
	Name = name;
	Value = value;
}

void Unet::LobbyDataContainer::SetData(const char* name, const std::string &value)
{
	//TODO: Consider uncommenting?
	//if (!strncmp(name, "unet-", 5)) {
	//	return;
	//}

	bool found = false;
	for (auto &data : m_data) {
		if (data.Name == name) {
			data.Value = value;
			found = true;
			break;
		}
	}

	if (!found) {
		LobbyData newData;
		newData.Name = name;
		newData.Value = value;
		m_data.emplace_back(newData);
	}
}

std::string Unet::LobbyDataContainer::GetData(const char* name) const
{
	for (auto &data : m_data) {
		if (data.Name == name) {
			return data.Value;
		}
	}
	return "";
}

json Unet::LobbyDataContainer::SerializeData() const
{
	json ret;
	for (auto &pair : m_data) {
		ret[pair.Name] = pair.Value;
	}
	return ret;
}

void Unet::LobbyDataContainer::DeserializeData(const json &js)
{
	for (auto &pair : js.items()) {
		bool found = false;
		for (auto &data : m_data) {
			if (data.Name == pair.key()) {
				data.Value = pair.value().get<std::string>();
				found = true;
				break;
			}
		}

		if (!found) {
			m_data.emplace_back(LobbyData(pair.key(), pair.value().get<std::string>()));
		}
	}
}
