#pragma once

#include <Unet_common.h>

namespace Unet
{
	enum class LobbyPacketType : uint8_t
	{
		// Sent by the client via all services to announce our Guid and verify our associated Service ID to the host
		Handshake,

		// Sent by the client via the primary service to announce basic member data such as the player name
		Hello,

		// Sent by the server to give basic lobby information, which readies the client
		LobbyInfo,

		// Sent by the server to announce a new member has joined
		MemberInfo,
		// Sent by the server to announce a member has left
		MemberLeft,
		// Sent by the server telling a member to leave (TODO: Force player to leave if they don't disconnect after a timeout)
		MemberKick,

		// Sent by the server to announce that a member has connected with a new service ID
		MemberNewService,

		// Sent by the server to announce changed lobby data
		LobbyData,
		// Sent by the server to announce removed lobby data
		LobbyDataRemoved,
		// Sent by the client to announce they're changing their own member lobby data
		// Sent by the server to announce changed member lobby data
		LobbyMemberData,
		// Sent by the client to announce they're removing their own member lobby data
		// Sent by the server to announce removed member lobby data
		LobbyMemberDataRemoved,

		// Sent by the client to announce that they've added a file available for download
		// Sent by the server to announce that a new file is available for download by a client
		LobbyFileAdded,
		// Sent by the client to announce that they've removed a file available for download
		// Sent by the server to announce that a file has been removed for downloading by a client
		LobbyFileRemoved,

		LobbyFileRequested,
		LobbyFileData,
	};
}
