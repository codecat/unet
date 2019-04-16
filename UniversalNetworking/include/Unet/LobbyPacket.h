#pragma once

#include <Unet_common.h>

namespace Unet
{
	enum class LobbyPacketType : uint8_t
	{
		/// Sent by the client via all services to announce our Guid and verify our associated Service ID to the host
		Handshake,

		/// Sent by the client via the primary service to announce basic member data such as the player name
		Hello,

		/// Sent by the server to give basic lobby information, which readies the client
		LobbyInfo,

		/// Sent by the server to announce a new member has joined
		MemberInfo,
		/// Sent by the server to announce a member has left
		MemberLeft,
		/// Sent by the server telling a member to leave (TODO: Force player to leave if they don't disconnect after a timeout)
		MemberKick,

		/// Sent by the server to announce that a member has connected with a new service ID
		MemberNewService,

		/// Sent by the server to announce changed lobby data
		LobbyData,
		/// Sent by the server to announce changed member lobby data
		LobbyMemberData,
	};
}
