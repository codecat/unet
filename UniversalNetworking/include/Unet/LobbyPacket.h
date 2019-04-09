#pragma once

#include <Unet_common.h>

namespace Unet
{
	enum class LobbyPacketType : uint8_t
	{
		/// Sent via all services to announce our Guid and verify our associated Service ID to the host
		Handshake,

		/// Sent via the primary service to announce basic member data such as the player name
		Hello,

		/// Sent by the server to give basic lobby information, which readies the client
		LobbyInfo,

		/// Sent by the server the indicate that a member has connected with a new service ID
		MemberNewService,
	};

	/*
	class LobbyPacket
	{
	public:
	};
	*/
}
