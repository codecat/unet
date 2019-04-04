#include <cstdio>
#include <cstring>

#include <iostream>

#include <Unet.h>

#include "termcolor.hpp"
#define LOG_TYPE(prefix, func) func(); printf("[" prefix "] "); termcolor::reset()
#define LOG_ERROR(fmt, ...) LOG_TYPE("ERROR", termcolor::red); printf(fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_TYPE(" WARN", termcolor::yellow); printf(fmt "\n", ##__VA_ARGS__)
#if defined(DEBUG)
# define LOG_DEBUG(fmt, ...) LOG_TYPE("DEBUG", termcolor::blue); printf(fmt "\n", ##__VA_ARGS__)
#else
# define LOG_DEBUG(fmt, ...)
#endif

#define S2_IMPL
#include "s2string.h"

#include <steam/steam_api.h>
#include <galaxy/GalaxyApi.h>

#if defined(_MSC_VER)
#include <Windows.h>
#endif

static Unet::Context* g_ctx = nullptr;
static bool g_keepRunning = true;
static Unet::LobbyListResult g_lastLobbyList;

static bool g_steamEnabled = false;
static bool g_galaxyEnabled = false;

class TestCallbacks : public Unet::Callbacks
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
		LOG_DEBUG("%s", str.c_str());
	}

	virtual void OnLobbyCreated(const Unet::CreateLobbyResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't create lobby!");
			return;
		}

		auto &info = result.CreatedLobby->GetInfo();
		LOG_INFO("Lobby created: \"%s\"", info.Name.c_str());
	}

	virtual void OnLobbyList(const Unet::LobbyListResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't get lobby list!");
			return;
		}

		g_lastLobbyList = result;

		LOG_INFO("%d lobbies:", (int)result.Lobbies.size());
		for (size_t i = 0; i < result.Lobbies.size(); i++) {
			auto &lobbyInfo = result.Lobbies[i];
			LOG_INFO("  [%d] \"%s\" (max %d)", (int)i, lobbyInfo.Name.c_str(), lobbyInfo.MaxPlayers);
			for (auto &entry : lobbyInfo.EntryPoints) {
				LOG_INFO("    %s (0x%08llX)", Unet::GetServiceNameByType(entry.Service), entry.ID);
			}
		}
	}

	virtual void OnLobbyJoined(const Unet::LobbyJoinResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			LOG_ERROR("Couldn't join lobby!");
			return;
		}

		auto &info = result.JoinedLobby->GetInfo();
		LOG_INFO("Joined lobby: \"%s\"", info.Name.c_str());
	}

	virtual void OnLobbyLeft(const Unet::LobbyLeftResult &result) override
	{
		const char* reasonStr = "Undefined";
		switch (result.Reason) {
		case Unet::LeaveReason::UserLeave: reasonStr = "User leave"; break;
		case Unet::LeaveReason::Disconnected: reasonStr = "Lost connection"; break;
		}
		LOG_INFO("Left lobby: %s", reasonStr);
	}
};

static void RunCallbacks()
{
	if (g_steamEnabled) {
		SteamAPI_RunCallbacks();
	}

	if (g_galaxyEnabled) {
		try {
			galaxy::api::ProcessData();
		} catch (const galaxy::api::IError &error) {
			LOG_ERROR("Failed to run Galaxy callbacks: %s", error.GetMsg());
		}
	}

	g_ctx->RunCallbacks();
}

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
		uint32 secretData = 0;
		SteamAPICall_t hSteamAPICall = SteamUser()->RequestEncryptedAppTicket(&secretData, sizeof(secretData));
		g_steamTicketCallback.m_callback.Set(hSteamAPICall, &g_steamTicketCallback, &SteamTicketCallback::OnCallback);
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
		LOG_INFO("  enable <name> [...] - Enables a service by the given name, including optional parameters");
		LOG_INFO("  status              - Prints current network status");
		LOG_INFO("  create [name]       - Creates a public lobby");
		LOG_INFO("  list                - Requests all available lobbies");
		LOG_INFO("  data [num]          - Show all lobby data by the number in the list, or the current lobby");
		LOG_INFO("  join <num>          - Joins a lobby by the number in the list");
		LOG_INFO("  leave               - Leaves the current lobby or cancels the join request");
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

			s2::string strLine(line);
			HandleCommand(strLine.trim());
		}

		fclose(fh);

	} else if (parse[0] == "enable" && parse.len() >= 2) {
		auto serviceName = parse[1];
		auto serviceType = Unet::GetServiceTypeByName(serviceName);
		if (!g_steamEnabled && serviceType == Unet::ServiceType::Steam && parse.len() == 3) {
			InitializeSteam(parse[2]);

			if (g_steamEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Steam);
			}

		} else if (!g_galaxyEnabled && serviceType == Unet::ServiceType::Galaxy && parse.len() == 4) {
			InitializeGalaxy(parse[2], parse[3]);

			if (g_galaxyEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Galaxy);
			}

		} else {
			LOG_ERROR("Unable to find service by the name of '%s' that takes %d parameters", serviceName.c_str(), (int)parse.len() - 2);
		}

	} else if (parse[0] == "status") {
		auto status = g_ctx->GetStatus();
		const char* statusStr = "Undefined";
		switch (status) {
		case Unet::ContextStatus::Idle: statusStr = "Idle"; break;
		case Unet::ContextStatus::Connecting: statusStr = "Connecting"; break;
		case Unet::ContextStatus::Connected: statusStr = "Connected"; break;
		}

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
				LOG_INFO("    %s (0x%08llX)", Unet::GetServiceNameByType(entry.Service), entry.ID);
			}
		}

	} else if (parse[0] == "create") {
		std::string name = "Unet Test Lobby";
		if (parse.len() == 2) {
			name = parse[1];
		}
		g_ctx->CreateLobby(Unet::LobbyPrivacy::Public, 16, name.c_str());

	} else if (parse[0] == "list") {
		g_ctx->GetLobbyList();

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
			lobbyData = g_ctx->GetLobbyData(lobbyInfo);

		} else {
			auto currentLobby = g_ctx->CurrentLobby();
			if (currentLobby == nullptr) {
				LOG_ERROR("Not in a lobby. Use \"data <num>\" instead.");
				return;
			}

			lobbyData = currentLobby->GetData();
		}

		LOG_INFO("%d keys:", (int)lobbyData.size());
		for (auto &data : lobbyData) {
			LOG_INFO("  \"%s\" = \"%s\"", data.Name.c_str(), data.Value.c_str());
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

	} else if (parse[0] == "leave") {
		g_ctx->LeaveLobby();

	} else {
		LOG_ERROR("Unknown command \"%s\"! Try \"help\".", parse[0].c_str());
	}
}

static s2::string ReadLine()
{
	auto currentLobby = g_ctx->CurrentLobby();
	if (currentLobby != nullptr) {
		auto &lobbyInfo = currentLobby->GetInfo();
		std::cout << "[" << lobbyInfo.Name << "] ";
	}

	std::cout << "> ";

	std::string line;
	std::getline(std::cin, line);
	return line.c_str();
}

int main(int argc, const char* argv[])
{
	g_ctx = new Unet::Context;
	g_ctx->SetCallbacks(new TestCallbacks);

	std::vector<s2::string> delayedCommands;

	for (int i = 1; i < argc; i++) {
		s2::string arg = argv[i];

		if (arg == "--steam" && i + 1 < argc) {
			const char* appIdStr = argv[++i];
			InitializeSteam(appIdStr);

			if (g_steamEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Steam);
			}
			continue;
		}

		if (arg == "--galaxy" && i + 2 < argc) {
			const char* clientId = argv[++i];
			const char* clientSecret = argv[++i];
			InitializeGalaxy(clientId, clientSecret);

			if (g_galaxyEnabled) {
				g_ctx->EnableService(Unet::ServiceType::Galaxy);
			}
			continue;
		}

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
		HandleCommand(ReadLine());
		RunCallbacks();
		printf("\n");
	}

	delete g_ctx;

	SteamAPI_Shutdown();

	if (g_authListener != nullptr) {
		delete g_authListener;
	}
	galaxy::api::Shutdown();

	return 0;
}
