#include <Unet_common.h>
#include <Unet/LobbyData.h>
#include <Unet/ServiceType.h>

void Unet::LobbyDataContainer::SetData(const char* name, const std::string &value)
{
	//TODO: Consider uncommenting?
	//if (strncmp(name, "unet-", 5) != 0) {
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
	//}
}

std::string Unet::LobbyDataContainer::GetData(const char* name)
{
	for (auto &data : m_data) {
		if (data.Name == name) {
			return data.Value;
		}
	}
	return "";
}

const std::vector<Unet::LobbyData> &Unet::LobbyDataContainer::GetAllData()
{
	return m_data;
}
