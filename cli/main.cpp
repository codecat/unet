#include <cstdio>
#include <cstring>
#include <ctime>

#include <iostream>

#include <Unet.h>

#define S2_IMPL
#include "s2string.h"

#if defined(UNET_MODULE_STEAM)
#	include <steam/steam_api.h>
#endif

#if defined(UNET_MODULE_GALAXY)
#	include <galaxy/GalaxyApi.h>
#endif

#if defined(UNET_MODULE_ENET)
#	include <enet/enet.h>
#endif

#if defined(PLATFORM_WINDOWS)
#include <Windows.h>
#else
#include <unistd.h>
#include <poll.h>
#endif

#include "termcolor.hpp"
#define LOG_TYPE(prefix, func) func(); printf("[" prefix "] "); termcolor::reset()
#define LOG_ERROR(fmt, ...) LOG_TYPE("ERROR", termcolor::red); printf(fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_FROM_CALLBACK(fmt, ...) LOG_TYPE("CALLBACK", termcolor::cyan); printf(fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_TYPE("WARN", termcolor::yellow); printf(fmt "\n", ##__VA_ARGS__)
#if defined(DEBUG)
# define LOG_DEBUG(fmt, ...) LOG_TYPE("DEBUG", termcolor::blue); printf(fmt "\n", ##__VA_ARGS__)
#else
# define LOG_DEBUG(fmt, ...)
#endif

static Unet::IContext* g_ctx = nullptr;
static bool g_keepRunning = true;
static Unet::LobbyListResult g_lastLobbyList;

static bool g_steamEnabled = false;
static bool g_galaxyEnabled = false;
static bool g_enetEnabled = false;

class TestCallbacks : public Unet::ICallbacks
{
public:
	virtual void OnLogError(const std::string &str) override
	{
		LOG_ERROR("%s", str.c_str());
	}

	virtual void OnLogWarn(const std::string &str) override
	{
		LOG_WARN("%s", str.c_str());
	}

	virtual void OnLogInfo(const std::string &str) override
	{
		LOG_INFO("%s", str.c_str());
	}

	virtual void OnLogDebug(const std::string &str) override
	{
#if defined(DEBUG)
		LOG_DEBUG("%s", str.c_str());
#endif
	}

	virtual void OnLobbyCreated(const Unet::CreateLobbyResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't create lobby!");
			return;
		}

		auto &info = result.CreatedLobby->GetInfo();
		LOG_FROM_CALLBACK("Lobby created: \"%s\"", info.Name.c_str());
	}

	virtual void OnLobbyList(const Unet::LobbyListResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't get lobby list!");
			return;
		}

		g_lastLobbyList = result;

		LOG_FROM_CALLBACK("%d lobbies: (%d filtered)", (int)result.Lobbies.size(), result.NumFiltered);
		for (size_t i = 0; i < result.Lobbies.size(); i++) {
			auto &lobbyInfo = result.Lobbies[i];
			LOG_FROM_CALLBACK("  [%d] \"%s\" (max %d)", (int)i, lobbyInfo.Name.c_str(), lobbyInfo.MaxPlayers);
			for (auto &entry : lobbyInfo.EntryPoints) {
				LOG_FROM_CALLBACK("    %s (0x%016" PRIx64 ")", Unet::GetServiceNameByType(entry.Service), entry.ID);
			}
		}
	}

	virtual void OnLobbyInfoFetched(const Unet::LobbyInfoFetchResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't fetch lobby info!");
			return;
		}

		LOG_FROM_CALLBACK("Fetched lobby info:");
		LOG_FROM_CALLBACK("    Is hosting: %s", result.Info.IsHosting ? "yes" : "no");
		LOG_FROM_CALLBACK("       Privacy: %s", result.Info.Privacy == Unet::LobbyPrivacy::Public ? "public" : "private");
		LOG_FROM_CALLBACK("       Players: %d / %d", result.Info.NumPlayers, result.Info.MaxPlayers);
		std::string lobbyGuid = result.Info.UnetGuid.str();
		LOG_FROM_CALLBACK("          Guid: %s", lobbyGuid.c_str());
		LOG_FROM_CALLBACK("          Name: \"%s\"", result.Info.Name.c_str());
		LOG_FROM_CALLBACK("  Entry points: %d", (int)result.Info.EntryPoints.size());
		for (auto& entry : result.Info.EntryPoints) {
			LOG_FROM_CALLBACK("    %s (0x%016" PRIx64 ")", Unet::GetServiceNameByType(entry.Service), entry.ID);
		}
	}

	virtual void OnLobbyJoined(const Unet::LobbyJoinResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't join lobby!");
			return;
		}

		auto &info = result.JoinedLobby->GetInfo();
		LOG_FROM_CALLBACK("Joined lobby: \"%s\"", info.Name.c_str());
	}

	virtual void OnLobbyLeft(const Unet::LobbyLeftResult &result) override
	{
		const char* reasonStr = "Undefined";
		switch (result.Reason) {
		case Unet::LeaveReason::UserLeave: reasonStr = "User leave"; break;
		case Unet::LeaveReason::Disconnected: reasonStr = "Disconnected"; break;
		case Unet::LeaveReason::Kicked: reasonStr = "Kicked"; break;
		}
		LOG_FROM_CALLBACK("Left lobby: %s", reasonStr);
	}

	virtual void OnLobbyNameChanged(const std::string &oldname, const std::string &newname) override
	{
		LOG_FROM_CALLBACK("Lobby name changed: \"%s\" => \"%s\"", oldname.c_str(), newname.c_str());
	}

	virtual void OnLobbyMaxPlayersChanged(int oldamount, int newamount) override
	{
		LOG_FROM_CALLBACK("Max players changed: %d => %d", oldamount, newamount);
	}

	virtual void OnLobbyPlayerJoined(Unet::LobbyMember* member) override
	{
		LOG_FROM_CALLBACK("Player joined: %s", member->Name.c_str());
	}

	virtual void OnLobbyPlayerLeft(Unet::LobbyMember* member) override
	{
		LOG_FROM_CALLBACK("Player left: %s", member->Name.c_str());
	}

	virtual void OnLobbyDataChanged(const std::string &name) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto value = currentLobby->GetData(name);
		if (value == "") {
			LOG_FROM_CALLBACK("Lobby data removed: \"%s\"", name.c_str());
		} else {
			LOG_FROM_CALLBACK("Lobby data changed: \"%s\" => \"%s\"", name.c_str(), value.c_str());
		}
	}

	virtual void OnLobbyMemberDataChanged(Unet::LobbyMember* member, const std::string &name) override
	{
		auto value = member->GetData(name);
		if (value == "") {
			LOG_FROM_CALLBACK("Lobby member data removed: \"%s\"", name.c_str());
		} else {
			LOG_FROM_CALLBACK("Lobby member data changed: \"%s\" => \"%s\"", name.c_str(), value.c_str());
		}
	}

	virtual void OnLobbyMemberNameChanged(Unet::LobbyMember* member, const std::string &oldname) override
	{
		LOG_FROM_CALLBACK("Lobby member changed their name: \"%s\" => \"%s\"", oldname.c_str(), member->Name.c_str());
	}

	virtual void OnLobbyFileAdded(Unet::LobbyMember* member, const Unet::LobbyFile* file) override
	{
		LOG_FROM_CALLBACK("%s added file \"%s\"", member->Name.c_str(), file->m_filename.c_str());
	}

	virtual void OnLobbyFileRemoved(Unet::LobbyMember* member, const std::string &filename) override
	{
		LOG_FROM_CALLBACK("%s removed file \"%s\"", member->Name.c_str(), filename.c_str());
	}

	virtual void OnLobbyFileRequested(Unet::LobbyMember* receiver, const Unet::LobbyFile* file) override
	{
		LOG_FROM_CALLBACK("%s requested our file \"%s\" for download", receiver->Name.c_str(), file->m_filename.c_str());
	}

	virtual void OnLobbyFileDataSendProgress(const Unet::OutgoingFileTransfer& transfer) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

		auto file = localMember->GetFile(transfer.FileHash);
		auto receiver = currentLobby->GetMember(transfer.MemberPeer);

		LOG_FROM_CALLBACK("Sending file \"%s\" to %s: %.1f%%", file->m_filename.c_str(), receiver->Name.c_str(), file->GetPercentage(transfer) * 100.0);
	}

	virtual void OnLobbyFileDataSendFinished(const Unet::OutgoingFileTransfer& transfer) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

		auto file = localMember->GetFile(transfer.FileHash);
		auto receiver = currentLobby->GetMember(transfer.MemberPeer);

		LOG_FROM_CALLBACK("Completed sending file \"%s\" to %s!", file->m_filename.c_str(), receiver->Name.c_str());
	}

	virtual void OnLobbyFileDataReceiveProgress(Unet::LobbyMember* sender, const Unet::LobbyFile* file) override
	{
		LOG_FROM_CALLBACK("Receiving file \"%s\" from %s: %.1f%%", file->m_filename.c_str(), sender->Name.c_str(), file->GetPercentage() * 100.0);
	}

	virtual void OnLobbyFileDataReceiveFinished(Unet::LobbyMember* sender, const Unet::LobbyFile* file, bool isValid) override
	{
		LOG_FROM_CALLBACK("Completed receiving file \"%s\" from %s! Valid: %s", file->m_filename.c_str(), sender->Name.c_str(), isValid ? "yes" : "no");
	}

	virtual void OnLobbyChat(Unet::LobbyMember* sender, const char* text) override
	{
		if (sender == nullptr) {
			LOG_FROM_CALLBACK("> %s", text);
		} else {
			LOG_FROM_CALLBACK("> %s: %s", sender->Name.c_str(), text);
		}
	}
};

static void RunCallbacks()
{
#if defined(UNET_MODULE_STEAM)
	if (g_steamEnabled) {
		SteamAPI_RunCallbacks();
	}
#endif

#if defined(UNET_MODULE_GALAXY)
	if (g_galaxyEnabled) {
		try {
			galaxy::api::ProcessData();
		} catch (const galaxy::api::IError &error) {
			LOG_ERROR("Failed to run Galaxy callbacks: %s", error.GetMsg());
		}
	}
#endif

	g_ctx->RunCallbacks();

	while (g_ctx->IsMessageAvailable(0)) {
		auto msg = g_ctx->ReadMessage(0);
		auto member = g_ctx->CurrentLobby()->GetMember(msg->m_peer);
		if (member == nullptr) {
			LOG_ERROR("Received message from a %s ID 0x%016" PRIx64, Unet::GetServiceNameByType(msg->m_peer.Service), msg->m_peer.ID);
		} else {
			LOG_INFO("Received message on channel %d: 0x%X bytes from %s ID 0x%016" PRIx64 " (%s)", msg->m_channel, (uint32_t)msg->m_size, Unet::GetServiceNameByType(msg->m_peer.Service), msg->m_peer.ID, member->Name.c_str());
		}
	}
}

#if defined(UNET_MODULE_STEAM)
static void InitializeSteam(const char* appId)
{
	LOG_INFO("Enabling Steam service (App ID %s)", appId);

#if defined(PLATFORM_WINDOWS)
	SetEnvironmentVariableA("SteamAppId", appId);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
	setenv("SteamAppId", appId, 1);
#endif

	g_steamEnabled = SteamAPI_Init();
	if (!g_steamEnabled) {
		LOG_ERROR("Failed to initialize Steam API!");
	}
}
#endif

#if defined(UNET_MODULE_GALAXY)
class GalaxyAuthListener : public galaxy::api::GlobalAuthListener
{
public:
	virtual void OnAuthSuccess() override
	{
		LOG_INFO("[Galaxy] Signed in successfully");
	}

	virtual void OnAuthFailure(FailureReason failureReason) override
	{
		LOG_ERROR("[Galaxy] Failed to sign in, error %d", (int)failureReason);
		g_galaxyEnabled = false;
	}

	virtual void OnAuthLost() override
	{
		LOG_ERROR("[Galaxy] Authentication lost");
		g_galaxyEnabled = false;
	}
};
static GalaxyAuthListener* g_authListener = nullptr;
#endif

#if defined(UNET_MODULE_GALAXY) && defined(UNET_MODULE_STEAM)
class SteamTicketCallback
{
public:
	CCallResult<SteamTicketCallback, EncryptedAppTicketResponse_t> m_callback;

	void OnCallback(EncryptedAppTicketResponse_t* result, bool bIOFailure)
	{
		uint8 steamAppTicket[1024];
		memset(steamAppTicket, 0, sizeof(steamAppTicket));

		uint32 ticketSz;
		if (!SteamUser()->GetEncryptedAppTicket(steamAppTicket, sizeof(steamAppTicket), &ticketSz)) {
			LOG_ERROR("[Galaxy] Failed to get encrypted app ticket from Steam");
			g_galaxyEnabled = false;
			return;
		}

		const char* personaName = SteamFriends()->GetPersonaName();

		try {
			galaxy::api::User()->SignInSteam(steamAppTicket, ticketSz, personaName);
		} catch (const galaxy::api::IError &error) {
			LOG_ERROR("[Galaxy] Failed to begin Steam sign in: %s", error.GetMsg());
			g_galaxyEnabled = false;
		}
	}
};
static SteamTicketCallback g_steamTicketCallback;
#endif

#if defined(UNET_MODULE_GALAXY)
static void InitializeGalaxy(const char* clientId, const char* clientSecret)
{
	LOG_INFO("Enabling Galaxy service (client ID %s)", clientId);

	g_authListener = new GalaxyAuthListener;

	try {
		galaxy::api::Init(galaxy::api::InitOptions(clientId, clientSecret));
	} catch (const galaxy::api::IError &error) {
		LOG_ERROR("Failed to initiailize Galaxy API: %s", error.GetMsg());
	}

	g_galaxyEnabled = true;

	if (g_steamEnabled) {
#if defined(UNET_MODULE_STEAM)
		uint32 secretData = 0;
		SteamAPICall_t hSteamAPICall = SteamUser()->RequestEncryptedAppTicket(&secretData, sizeof(secretData));
		g_steamTicketCallback.m_callback.Set(hSteamAPICall, &g_steamTicketCallback, &SteamTicketCallback::OnCallback);
#endif
	} else {
		try {
			galaxy::api::User()->SignInGalaxy(true);
		} catch (const galaxy::api::IError &error) {
			LOG_ERROR("Failed to sign in to Galaxy: %s", error.GetMsg());
		}
	}

	LOG_INFO("Waiting for Galaxy sign in...");
	while (g_galaxyEnabled && !galaxy::api::User()->SignedIn()) {
		RunCallbacks();
	}
}
#endif

#if defined(UNET_MODULE_ENET)
static void InitializeEnet()
{
	LOG_INFO("Initializing Enet");

	g_enetEnabled = true;
	enet_initialize();
}
#endif

static void UnetSleep(int ms)
{
#if defined(PLATFORM_WINDOWS)
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

static bool IsKeyPressed()
{
#if defined(PLATFORM_WINDOWS)
	return WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), 0) == WAIT_OBJECT_0;
#else
	struct pollfd pls[1];
	pls[0].fd = STDIN_FILENO;
	pls[0].events = POLLIN | POLLPRI;
	return poll(pls, 1, 0) > 0;
#endif
}

static void HandleCommand(const s2::string &line)
{
	auto parse = line.commandlinesplit();

	if (parse[0] == "") {
		return;

	} else if (parse[0] == "exit") {
		g_keepRunning = false;

	} else if (parse[0] == "help") {
		LOG_INFO("Available commands:");
		LOG_INFO("  exit");
		LOG_INFO("  help");
		LOG_INFO("  run <filename>      - Runs commands from a file");
		LOG_INFO("");
		LOG_INFO("  enable <name> [...] - Enables a service by the given name, including optional parameters");
		LOG_INFO("  primary <name>      - Changes the primary service");
		LOG_INFO("  persona <name>      - Sets your persona name");
		LOG_INFO("");
		LOG_INFO("  status              - Prints current network status");
		LOG_INFO("  wait                - Keeps running callbacks until a key is pressed or when disconnected");
		LOG_INFO("");
		LOG_INFO("  create [name]       - Creates a public lobby");
		LOG_INFO("  fetch <service> <id> - Fetches lobby info for an ID for a service");
		LOG_INFO("  list                - Requests all available lobbies");
		LOG_INFO("  join <num>          - Joins a lobby by the number in the list");
		LOG_INFO("  joinleave <num>     - Joins a lobby and immediately leaves it (for cancelation testing)");
		LOG_INFO("  connect <ip> [port] - Connect to a server directly by IP address (if enet is enabled)");
		LOG_INFO("  leave               - Leaves the current lobby with all services");
		LOG_INFO("  outage <service>    - Simulates a service outage");
		LOG_INFO("");
		LOG_INFO("  setname <name>      - Sets the lobby name (only available on the host)");
		LOG_INFO("  data [num]          - Shows all lobby data by the number in the list, or the current lobby");
		LOG_INFO("  memberdata [peer]   - Shows all lobby member data for the given peer, or the local member");
		LOG_INFO("  setdata <name> <value> - Sets lobby data (only available on the host)");
		LOG_INFO("  remdata <name>      - Removes lobby data (only available on the host)");
		LOG_INFO("  setmemberdata <peer> <name> <value> - Sets member lobby data (only available on the host and the local peer)");
		LOG_INFO("  remmemberdata <peer> <name> - Removes member lobby data (only available on the host and the local peer)");
		LOG_INFO("  kick <peer>         - Kicks the given peer with an optional reason");
		LOG_INFO("  chat <message>      - Sends a chat message to the lobby");
		LOG_INFO("");
		LOG_INFO("  addfile <filename>  - Adds a file to the available lobby files for the local member");
		LOG_INFO("  delfile <filename>  - Removes a file from the available lobby files for the local member");
		LOG_INFO("  download <peer> <filename> - Requests a file from the given peer and waits for its completion");
		LOG_INFO("");
		LOG_INFO("  send <peer> <num>   - Sends the given peer a reliable packet with a number of random bytes on channel 0");
		LOG_INFO("  sendu <peer> <num>  - Sends the given peer an unreliable packet with a number of random bytes on channel 0");
		LOG_INFO("");
		LOG_INFO("  stress <peer> <sec> - Stress test the given peer for the given amount of seconds.");
		LOG_INFO("");
		LOG_INFO("Or just hit Enter to run callbacks.");

	} else if (parse[0] == "run" && parse.len() == 2) {
		auto filename = parse[1];
		FILE* fh = fopen(filename, "rb");
		if (fh == nullptr) {
			LOG_ERROR("Couldn't find file \"%s\"", filename.c_str());
			return;
		}

		char lineBuffer[1024];
		while (!feof(fh)) {
			char* line = fgets(lineBuffer, 1024, fh);
			if (line == nullptr) {
				break;
			}

			s2::string strLine = s2::string(line).trim();
			if (strLine == "" || strLine.startswith("//")) {
				continue;
			}

			HandleCommand(strLine);
		}

		fclose(fh);

	} else if (parse[0] == "enable" && parse.len() >= 2) {
		auto serviceName = parse[1];
		auto serviceType = Unet::GetServiceTypeByName(serviceName);

#if defined(UNET_MODULE_STEAM)
		if (!g_steamEnabled && serviceType == Unet::ServiceType::Steam && parse.len() == 3) {
			InitializeSteam(parse[2]);

			if (g_steamEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Steam);
			}
		} else
#endif

#if defined(UNET_MODULE_GALAXY)
		if (!g_galaxyEnabled && serviceType == Unet::ServiceType::Galaxy && parse.len() == 4) {
			InitializeGalaxy(parse[2], parse[3]);

			if (g_galaxyEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Galaxy);
			}
		} else
#endif

#if defined(UNET_MODULE_ENET)
		if (!g_enetEnabled && serviceType == Unet::ServiceType::Enet) {
			InitializeEnet();

			if (g_enetEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Enet);
			}
		} else
#endif

		// Note: We need this condition so that the above else clauses make sense, even if there are no modules at all!
		if (true) {
			LOG_ERROR("Unable to find service by the name of '%s' that takes %d parameters", serviceName.c_str(), (int)parse.len() - 2);
		}

	} else if (parse[0] == "primary" && parse.len() == 2) {
		auto serviceName = parse[1];
		auto serviceType = Unet::GetServiceTypeByName(serviceName);
		if (serviceType == Unet::ServiceType::None) {
			LOG_ERROR("Unable to find service by the name of '%s'", serviceName.c_str());
		} else {
			g_ctx->SetPrimaryService(serviceType);
		}

	} else if (parse[0] == "persona" && parse.len() == 2) {
		auto nameString = parse[1];
		g_ctx->SetPersonaName(nameString.c_str());

	} else if (parse[0] == "status") {
		auto status = g_ctx->GetStatus();
		const char* statusStr = "Undefined";
		switch (status) {
		case Unet::ContextStatus::Idle: statusStr = "Idle"; break;
		case Unet::ContextStatus::Connecting: statusStr = "Connecting"; break;
		case Unet::ContextStatus::Connected: statusStr = "Connected"; break;
		}

		LOG_INFO("Local peer: %d", g_ctx->GetLocalPeer());

		auto localGuid = g_ctx->GetLocalGuid().str();
		LOG_INFO("Local Guid: %s", localGuid.c_str());

		LOG_INFO("Status: %s", statusStr);

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_INFO("  No lobby");
		} else {
			auto &lobbyInfo = currentLobby->GetInfo();

			LOG_INFO("  Lobby name: \"%s\"", lobbyInfo.Name.c_str());
			LOG_INFO("  Lobby host: %s", lobbyInfo.IsHosting ? "true" : "false");
			auto unetGuid = lobbyInfo.UnetGuid.str();
			LOG_INFO("  Lobby Guid: %s", unetGuid.c_str());

			LOG_INFO("  Entry points: %d", (int)lobbyInfo.EntryPoints.size());
			for (auto &entry : lobbyInfo.EntryPoints) {
				LOG_INFO("    %s (0x%016" PRIx64 ")", Unet::GetServiceNameByType(entry.Service), entry.ID);
			}

			auto &members = currentLobby->GetMembers();
			LOG_INFO("  Members: %d", (int)members.size());
			for (auto member : members) {
				auto memberGuid = member->UnetGuid.str();
				LOG_INFO("    %d: \"%s\" (%s) (%s)", member->UnetPeer, member->Name.c_str(), member->Valid ? "Valid" : "Invalid", memberGuid.c_str());

				LOG_INFO("      Ping: %d", member->Ping);

				LOG_INFO("      %d datas", (int)member->m_data.size());

				LOG_INFO("      %d IDs:", (int)member->IDs.size());
				for (auto &id : member->IDs) {
					LOG_INFO("        %s (0x%016" PRIx64 ")%s", Unet::GetServiceNameByType(id.Service), id.ID, member->UnetPrimaryService == id.Service ? " Primary" : "");
				}

				LOG_INFO("      %d files:", (int)member->Files.size());
				for (auto file : member->Files) {
					LOG_INFO("        %s (%d / %d bytes)", file->m_filename.c_str(), (int)file->m_availableSize, (int)file->m_size);
				}
			}
		}

	} else if (parse[0] == "wait") {
		LOG_INFO("Entering wait mode. Press any key to stop.");

		while (true) {
			RunCallbacks();

			UnetSleep(1);

			if (IsKeyPressed()) {
				break;
			}

			if (g_ctx->GetStatus() == Unet::ContextStatus::Idle) {
				break;
			}
		}

	} else if (parse[0] == "create") {
		std::string name = "Unet Test Lobby";
		if (parse.len() == 2) {
			name = parse[1];
		}
		g_ctx->CreateLobby(Unet::LobbyPrivacy::Public, 16, name.c_str());

		LOG_INFO("Creating lobby");

		while (g_ctx->GetStatus() == Unet::ContextStatus::Connecting) {
			RunCallbacks();
		}

	} else if (parse[0] == "fetch" && parse.len() == 3) {
		auto serviceName = parse[1];
		auto serviceType = Unet::GetServiceTypeByName(serviceName);

		uint64_t id = strtoll(parse[2].c_str(), nullptr, 0);

		LOG_DEBUG("Fetching 0x%016" PRIx64, id);

		if (!g_ctx->FetchLobbyInfo(Unet::ServiceID(serviceType, id))) {
			LOG_ERROR("Unable to begin fetching lobby info.");
		}

	} else if (parse[0] == "list") {
		Unet::LobbyListFilter filter;
		g_ctx->GetLobbyList(filter);

	} else if (parse[0] == "setname" && parse.len() == 2) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby");
			return;
		}

		currentLobby->SetName(parse[1].c_str());

	} else if (parse[0] == "data") {
		std::vector<Unet::LobbyData> lobbyData;

		if (parse.len() == 2) {
			if (g_lastLobbyList.Code != Unet::Result::OK) {
				LOG_ERROR("Previous lobby list request failed! Use the \"list\" command again.");
				return;
			}

			int num = atoi(parse[1]);
			if (num < 0 || num >= (int)g_lastLobbyList.Lobbies.size()) {
				LOG_ERROR("Number %d is out of range of last lobby list!", num);
				return;
			}

			auto &lobbyInfo = g_lastLobbyList.Lobbies[num];
			lobbyData = g_lastLobbyList.GetLobbyData(lobbyInfo);

		} else {
			auto currentLobby = g_ctx->CurrentLobby();
			if (currentLobby == nullptr) {
				LOG_ERROR("Not in a lobby. Use \"data <num>\" instead.");
				return;
			}

			lobbyData = currentLobby->m_data;
		}

		LOG_INFO("%d keys:", (int)lobbyData.size());
		for (auto &data : lobbyData) {
			LOG_INFO("  \"%s\" = \"%s\"", data.Name.c_str(), data.Value.c_str());
		}

	} else if (parse[0] == "memberdata") {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		std::vector<Unet::LobbyData> lobbyData;

		int peer = g_ctx->GetLocalPeer();

		if (parse.len() == 2) {
			peer = atoi(parse[1]);
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Couldn't find member for peer %d", peer);
			return;
		}

		LOG_INFO("%d keys:", (int)member->m_data.size());
		for (auto &data : member->m_data) {
			LOG_INFO("  \"%s\" = \"%s\"", data.Name.c_str(), data.Value.c_str());
		}

	} else if (parse[0] == "setdata" && parse.len() == 3) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		if (!currentLobby->GetInfo().IsHosting) {
			LOG_ERROR("Lobby data can only be set by the host.");
			return;
		}

		s2::string name = parse[1];
		s2::string value = parse[2];

		currentLobby->SetData(name.c_str(), value.c_str());

	} else if (parse[0] == "remdata" && parse.len() == 2) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		if (!currentLobby->GetInfo().IsHosting) {
			LOG_ERROR("Lobby data can only be removed by the host.");
			return;
		}

		s2::string name = parse[1];

		currentLobby->RemoveData(name.c_str());

	} else if (parse[0] == "setmemberdata" && parse.len() == 4) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		int peer = atoi(parse[1]);
		if (!currentLobby->GetInfo().IsHosting && peer != g_ctx->GetLocalPeer()) {
			LOG_ERROR("You can't change data for this peer!");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		s2::string name = parse[2];
		s2::string value = parse[3];

		member->SetData(name.c_str(), value.c_str());

	} else if (parse[0] == "remmemberdata" && parse.len() == 3) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		int peer = atoi(parse[1]);
		if (!currentLobby->GetInfo().IsHosting && peer != g_ctx->GetLocalPeer()) {
			LOG_ERROR("You can't remove data for this peer!");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		s2::string name = parse[2];

		member->RemoveData(name.c_str());

	} else if (parse[0] == "addfile" && parse.len() == 2) {
		if (g_ctx->CurrentLobby() == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		s2::string filename = parse[1];
		g_ctx->AddFile(filename, filename);

		LOG_INFO("Added file \"%s\"", filename.c_str());

	} else if (parse[0] == "delfile" && parse.len() == 2) {
		if (g_ctx->CurrentLobby() == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		s2::string filename = parse[1];
		g_ctx->RemoveFile(filename);

		LOG_INFO("Removed file \"%s\"", filename.c_str());

	} else if (parse[0] == "download" && parse.len() == 3) {
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		int peer = atoi(parse[1]);
		s2::string filename = parse[2];

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		auto file = member->GetFile(filename.c_str());
		if (file == nullptr) {
			LOG_ERROR("Couldn't find file with name \"%s\" on this member!", filename.c_str());
			return;
		}

		g_ctx->RequestFile(member, file);

		LOG_INFO("File requested. Waiting for completion..");

		while (file->m_availableSize < file->m_size) {
			RunCallbacks();
		}

		if (file->IsValid()) {
			LOG_INFO("File received and valid!");
		} else {
			LOG_ERROR("File received, but it's not valid!");
		}

	} else if (parse[0] == "join" && parse.len() == 2) {
		if (g_lastLobbyList.Code != Unet::Result::OK) {
			LOG_ERROR("Previous lobby list request failed! Use the \"list\" command again.");
			return;
		}

		int num = atoi(parse[1]);
		if (num < 0 || num >= (int)g_lastLobbyList.Lobbies.size()) {
			LOG_INFO("Number %d is out of range of last lobby list!", num);
			return;
		}

		auto lobbyInfo = g_lastLobbyList.Lobbies[num];

		LOG_INFO("Joining \"%s\"", lobbyInfo.Name.c_str());
		g_ctx->JoinLobby(lobbyInfo);

		while (g_ctx->GetStatus() == Unet::ContextStatus::Connecting) {
			RunCallbacks();
		}

	} else if (parse[0] == "joinleave" && parse.len() == 2) {
		if (g_lastLobbyList.Code != Unet::Result::OK) {
			LOG_ERROR("Previous lobby list request failed! Use the \"list\" command again.");
			return;
		}

		int num = atoi(parse[1]);
		if (num < 0 || num >= (int)g_lastLobbyList.Lobbies.size()) {
			LOG_INFO("Number %d is out of range of last lobby list!", num);
			return;
		}

		auto lobbyInfo = g_lastLobbyList.Lobbies[num];

		LOG_INFO("Join-leaving \"%s\"", lobbyInfo.Name.c_str());
		g_ctx->JoinLobby(lobbyInfo);
		g_ctx->LeaveLobby();

	} else if (parse[0] == "connect" && parse.len() >= 2) {
		if (!g_enetEnabled) {
			LOG_ERROR("Enet is not enabled!");
			return;
		}

		s2::string strIP = parse[1];
		s2::string strPort = "4450";

		if (parse.len() == 3) {
			strPort = parse[2];
		}

		ENetAddress addr;
		enet_address_set_host(&addr, strIP);
		addr.port = (enet_uint16)atoi(strPort);
		g_ctx->JoinLobby(Unet::ServiceID(Unet::ServiceType::Enet, *(uint64_t*)&addr));

		while (g_ctx->GetStatus() == Unet::ContextStatus::Connecting) {
			RunCallbacks();
		}

	} else if (parse[0] == "leave") {
		g_ctx->LeaveLobby();

		LOG_INFO("Leaving lobby");

		while (g_ctx->GetStatus() == Unet::ContextStatus::Connected) {
			RunCallbacks();
		}

	} else if (parse[0] == "outage" && parse.len() == 2) {
		s2::string strService = parse[1];
		Unet::ServiceType service = Unet::GetServiceTypeByName(strService);

		if (service == Unet::ServiceType::None) {
			LOG_ERROR("Invalid service type!");
			return;
		}

		g_ctx->SimulateServiceOutage(service);

	} else if (parse[0] == "kick" && parse.len() == 2) {
		int peer = atoi(parse[1]);
		if (peer == g_ctx->GetLocalPeer()) {
			LOG_ERROR("You can't kick yourself!");
			return;
		}

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		g_ctx->KickMember(member);

	} else if (parse[0] == "chat" && parse.len() == 2) {
		g_ctx->SendChat(parse[1]);

	} else if (parse[0] == "send" && parse.len() == 3) {
		int peer = atoi(parse[1]);
		int num = atoi(parse[2]);

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		uint8_t* bytes = (uint8_t*)malloc(num);
		if (bytes != nullptr) {
			for (int i = 0; i < num; i++) {
				bytes[i] = (uint8_t)(rand() % 255);
			}
			g_ctx->SendTo(member, bytes, num, Unet::PacketType::Reliable);
			free(bytes);
		}

		LOG_INFO("0x%X reliable bytes sent to peer %d: \"%s\"!", num, member->UnetPeer, member->Name.c_str());

	} else if (parse[0] == "sendu" && parse.len() == 3) {
		int peer = atoi(parse[1]);
		int num = atoi(parse[2]);

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		uint8_t* bytes = (uint8_t*)malloc(num);
		if (bytes != nullptr) {
			for (int i = 0; i < num; i++) {
				bytes[i] = (uint8_t)(rand() % 255);
			}
			g_ctx->SendTo(member, bytes, num, Unet::PacketType::Unreliable);
			free(bytes);

			LOG_INFO("0x%X unreliable bytes sent to peer %d: \"%s\"!", num, member->UnetPeer, member->Name.c_str());
		}

	} else if (parse[0] == "stress" && parse.len() == 3) {
		int peer = atoi(parse[1]);
		int sec = atoi(parse[2]);

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			LOG_ERROR("Not in a lobby.");
			return;
		}

		auto member = currentLobby->GetMember(peer);
		if (member == nullptr) {
			LOG_ERROR("Peer ID %d does not belong to a member!", peer);
			return;
		}

		const size_t maxSmallSize = 200;
		const size_t maxMediumSize = 2000;
		const size_t maxBigSize = 1024 * 1024 * 3;

		uint8_t* randomBuffer = (uint8_t*)malloc(maxBigSize);

		time_t startTime = time(nullptr);
		while (true) {
			RunCallbacks();

			UnetSleep(100);

			time_t nowTime = time(nullptr);
			time_t timeSpent = nowTime - startTime;
			if (timeSpent > sec) {
				break;
			}

			Unet::PacketType type = Unet::PacketType::Unreliable;
			if ((rand() % 2) == 1) {
				// 50% chance to be a reliable packet
				type = Unet::PacketType::Reliable;
			}

			size_t maxSize = maxSmallSize;
			if (rand() % 100 > 70) {
				// 30% chance to be a medium packet
				maxSize = maxMediumSize;
			} else if (rand() % 100 > 90) {
				// 10% chance to be a big packet
				maxSize = maxBigSize;
			}

			size_t size;
			do {
				size = rand() % maxSize;
			} while (size == 0);

			for (size_t i = 0; i < size; i++) {
				randomBuffer[i] = (uint8_t)(rand() % 255);
			}

			g_ctx->SendTo(member, randomBuffer, size, type);
			LOG_DEBUG("Sent 0x%X bytes %sreliably", (int)size, type == Unet::PacketType::Unreliable ? "un" : "");
		}

		LOG_INFO("Done!");

		free(randomBuffer);

	} else {
		LOG_ERROR("Unknown command \"%s\"! Try \"help\".", parse[0].c_str());
	}
}

static s2::string ReadLine()
{
	tm date;
	time_t itime = time(nullptr);
#if defined(PLATFORM_WINDOWS)
	_localtime64_s(&date, &itime);
#else
	localtime_r(&itime, &date);
#endif
	printf("[%02d:%02d:%02d] ", date.tm_hour, date.tm_min, date.tm_sec);

	std::cout << "[" << g_ctx->GetPersonaName() << "] ";

	std::cout << "[";
	termcolor::cyan();

	auto status = g_ctx->GetStatus();
	switch (status) {
	case Unet::ContextStatus::Idle: std::cout << "Idle"; break;
	case Unet::ContextStatus::Connecting: std::cout << "Connecting"; break;
	case Unet::ContextStatus::Connected: std::cout << "Connected"; break;
	}

	termcolor::reset();
	std::cout << "] ";

	if (status == Unet::ContextStatus::Connected) {
		std::cout << "[";

		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby != nullptr) {
			auto &lobbyInfo = currentLobby->GetInfo();

			if (lobbyInfo.IsHosting) {
				termcolor::red();
				termcolor::bold();
				std::cout << "HOST ";
				termcolor::reset();
			}

			termcolor::green();
			std::cout << lobbyInfo.Name;

		} else {
			termcolor::red();
			std::cout << "No lobby!";
		}

		termcolor::reset();
		std::cout << "] ";
	}

	std::cout << "> ";

	std::string line;
	std::getline(std::cin, line);
	return line.c_str();
}

int main(int argc, const char* argv[])
{
	srand((unsigned int)time(nullptr));

#if defined(UNET_MODULE_STEAM)
	LOG_INFO("Enabled module: Steam");
#endif
#if defined(UNET_MODULE_GALAXY)
	LOG_INFO("Enabled module: Galaxy");
#endif
#if defined(UNET_MODULE_ENET)
	LOG_INFO("Enabled module: Enet");
#endif

	g_ctx = Unet::CreateContext();
	g_ctx->SetCallbacks(new TestCallbacks);

	std::vector<s2::string> delayedCommands;

	for (int i = 1; i < argc; i++) {
		s2::string arg = argv[i];

#if defined(UNET_MODULE_STEAM)
		if (arg == "--steam" && i + 1 < argc) {
			const char* appIdStr = argv[++i];
			InitializeSteam(appIdStr);

			if (g_steamEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Steam);
			}
			continue;
		}
#endif

#if defined(UNET_MODULE_GALAXY)
		if (arg == "--galaxy" && i + 2 < argc) {
			const char* clientId = argv[++i];
			const char* clientSecret = argv[++i];
			InitializeGalaxy(clientId, clientSecret);

			if (g_galaxyEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Galaxy);
			}
			continue;
		}
#endif

#if defined(UNET_MODULE_ENET)
		if (arg == "--enet") {
			InitializeEnet();

			if (g_enetEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Enet);
			}
			continue;
		}
#endif

		if (arg == "--primary" && i + 1 < argc) {
			g_ctx->SetPrimaryService(Unet::GetServiceTypeByName(argv[++i]));
			continue;
		}

		delayedCommands.emplace_back(arg);
	}

	RunCallbacks();

	for (auto &cmd : delayedCommands) {
		HandleCommand(cmd);
		printf("\n");
	}
	delayedCommands.clear();

	while (g_keepRunning) {
		if (feof(stdin)) {
			break;
		}
		HandleCommand(ReadLine());
		RunCallbacks();
		printf("\n");
	}

	Unet::DestroyContext(g_ctx);

#if defined(UNET_MODULE_STEAM)
	SteamAPI_Shutdown();
#endif

#if defined(UNET_MODULE_GALAXY)
	if (g_authListener != nullptr) {
		delete g_authListener;
	}
	galaxy::api::Shutdown();
#endif

	enet_deinitialize();

	return 0;
}
