#include <Unet_common.h>
#include <Unet/LobbyData.h>
#include <Unet/ServiceType.h>

Unet::LobbyData::LobbyData()
{
}

Unet::LobbyData::LobbyData(const std::string &name, const std::string &value)
{
	Name = name;
	Value = value;
}

bool Unet::LobbyDataContainer::SetData(const std::string &name, const std::string &value)
{
	return InternalSetData(name, value);
}

std::string Unet::LobbyDataContainer::GetData(const std::string &name) const
{
	for (auto &data : m_data) {
		if (data.Name == name) {
			return data.Value;
		}
	}
	return "";
}

bool Unet::LobbyDataContainer::RemoveData(const std::string &name)
{
	return InternalRemoveData(name);
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

bool Unet::LobbyDataContainer::InternalSetData(const std::string &name, const std::string &value)
{
	for (auto &data : m_data) {
		if (data.Name == name) {
			if (data.Value == value) {
				return false;
			}
			data.Value = value;
			return true;
		}
	}

	LobbyData newData;
	newData.Name = name;
	newData.Value = value;
	m_data.emplace_back(newData);
	return true;
}

bool Unet::LobbyDataContainer::InternalRemoveData(const std::string &name)
{
	auto it = std::find_if(m_data.begin(), m_data.end(), [&name](const LobbyData & pair) {
		return pair.Name == name;
	});

	if (it == m_data.end()) {
		return false;
	}

	m_data.erase(it);
	return true;
}
