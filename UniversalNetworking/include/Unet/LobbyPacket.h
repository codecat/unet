#pragma once

#include <Unet_common.h>

namespace Unet
{
	enum class LobbyPacketType : uint8_t
	{
		/*
		 * Client to host: Hello, this is my guid
		 * Host to client: Hello, here's some lobby info:
		 * - all lobby data
		 * - all lobby members
		 * - all lobby member data
		 */
		Hello,

		/*
		 * Client to host: Relay this packet to this ServiceID
		 * Host to client: This is a relay packet from this ServiceID
		 */
		Relay,
	};

	/*
	class LobbyPacket
	{
	public:
	};
	*/
}
