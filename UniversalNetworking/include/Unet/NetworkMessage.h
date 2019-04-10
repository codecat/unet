#pragma once

#include <Unet_common.h>
#include <Unet/ServiceID.h>

namespace Unet
{
	class NetworkMessage
	{
	public:
		ServiceID m_peer;
		int m_channel = 0;

		uint8_t* m_data;
		size_t m_size;

		//TODO: Implement packet re-assembly

	public:
		NetworkMessage(size_t size);
		NetworkMessage(uint8_t* data, size_t size);
		~NetworkMessage();
	};
}
