#include <Unet_common.h>
#include <Unet/NetworkMessage.h>

Unet::NetworkMessage::NetworkMessage(size_t size)
{
	m_size = size;
	m_data = (uint8_t*)malloc(size);
}

Unet::NetworkMessage::NetworkMessage(uint8_t* data, size_t size)
	: NetworkMessage(size)
{
	if (m_data != nullptr) {
		memcpy(m_data, data, size);
	}
}

Unet::NetworkMessage::~NetworkMessage()
{
	if (m_data != nullptr) {
		free(m_data);
	}
}
