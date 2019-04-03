#include <cstdio>
#include <cstring>

#include <iostream>

#include <Unet.h>

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
	virtual void OnLobbyCreated(const Unet::CreateLobbyResult &result) override
	{
		if (result.Result != Unet::Result::OK) {
			printf("Couldn't create lobby!\n");
			return;
		}

		auto &info = result.Lobby->GetInfo();
		printf("Lobby created: \"%s\"\n", info.Name.c_str());
	}

	virtual void OnLobbyList(const Unet::LobbyListResult &result) override
	{
		if (result.Result != Unet::Result::OK) {
			printf("Couldn't get lobby list!\n");
			return;
		}

		g_lastLobbyList = result;

		printf("%d lobbies:\n", (int)result.Lobbies.size());
		for (size_t i = 0; i < result.Lobbies.size(); i++) {
			auto &lobbyInfo = result.Lobbies[i];
			printf("  [%d] %s\n", i, lobbyInfo.Name.c_str());
		}
	}

	virtual void OnLobbyJoined(const Unet::LobbyJoinResult &result) override
	{
		if (result.Result != Unet::Result::OK) {
			printf("Couldn't join lobby!\n");
			return;
		}

		auto &info = result.Lobby->GetInfo();
		printf("Joined lobby: \"%s\"\n", info.Name.c_str());
	}

	virtual void OnLobbyLeft(const Unet::LobbyLeftResult &result) override
	{
		const char* reasonStr = "Undefined";
		switch (result.Reason) {
		case Unet::LeaveReason::UserLeave: reasonStr = "User leave"; break;
		case Unet::LeaveReason::Disconnected: reasonStr = "Lost connection"; break;
		}
		printf("Left lobby: %s\n", reasonStr);
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
			printf("Failed to run Galaxy callbacks: %s\n", error.GetMsg());
		}
	}

	g_ctx->RunCallbacks();
}

static void HandleCommand(const s2::string &line)
{
	auto parse = line.commandlinesplit();

	if (parse[0] == "") {
		return;

	} else if (parse[0] == "exit") {
		g_keepRunning = false;

	} else if (parse[0] == "help") {
		printf("Available commands:\n\n");
		printf("  exit\n");
		printf("  help\n");
		printf("  status        - Prints current network status\n");
		printf("  create        - Creates a public lobby\n");
		printf("  list          - Requests all available lobbies\n");
		printf("  join <num>    - Joins a lobby by the number in the list\n");
		printf("  leave         - Leaves the current lobby or cancels the join request\n");
		printf("\nOr just hit Enter to run callbacks.\n");

	} else if (parse[0] == "status") {
		auto status = g_ctx->GetStatus();
		const char* statusStr = "Undefined";
		switch (status) {
		case Unet::ContextStatus::Idle: statusStr = "Idle"; break;
		case Unet::ContextStatus::Connecting: statusStr = "Connecting"; break;
		case Unet::ContextStatus::Connected: statusStr = "Connected"; break;
		}

		printf("Status: %s\n", statusStr);
		auto currentLobby = g_ctx->CurrentLobby();
		if (currentLobby == nullptr) {
			printf("  No lobby\n");
		} else {
			auto &lobbyInfo = currentLobby->GetInfo();
			printf("  Lobby name: \"%s\"\n", lobbyInfo.Name.c_str());
			printf("  Entry points: %d\n", (int)lobbyInfo.EntryPoints.size());
			for (auto &entry : lobbyInfo.EntryPoints) {
				printf("    %s (0x%08llX)\n", Unet::GetServiceNameByType(entry.Service), entry.ID);
			}
		}

	} else if (parse[0] == "create") {
		g_ctx->CreateLobby(Unet::LobbyPrivacy::Public, 16);

	} else if (parse[0] == "list") {
		g_ctx->GetLobbyList();

	} else if (parse[0] == "join" && parse.len() == 2) {
		if (g_lastLobbyList.Result != Unet::Result::OK) {
			printf("Previous lobby list request failed! Use the \"list\" command again.\n");
			return;
		}

		int num = atoi(parse[1]);
		if (num < 0 || num >= (int)g_lastLobbyList.Lobbies.size()) {
			printf("Number %d is out of range of last lobby list!\n", num);
			return;
		}

		auto lobbyInfo = g_lastLobbyList.Lobbies[num];

		printf("Joining \"%s\"\n", lobbyInfo.Name.c_str());
		g_ctx->JoinLobby(lobbyInfo);

	} else if (parse[0] == "leave") {
		g_ctx->LeaveLobby();

	} else {
		printf("Unknown command \"%s\"! Try \"help\".\n", parse[0].c_str());
	}
}

static s2::string ReadLine()
{
	std::cout << "> ";

	std::string line;
	std::getline(std::cin, line);
	return line.c_str();
}

class GalaxyAuthListener : public galaxy::api::GlobalAuthListener
{
public:
	virtual void OnAuthSuccess() override
	{
		printf("[Galaxy] Signed in successfully\n");
	}

	virtual void OnAuthFailure(FailureReason failureReason) override
	{
		printf("[Galaxy] Failed to sign in, error %d\n", (int)failureReason);
		g_galaxyEnabled = false;
	}

	virtual void OnAuthLost() override
	{
		printf("[Galaxy] Authentication lost\n");
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
			printf("[Galaxy] Failed to get encrypted app ticket from Steam\n");
			g_galaxyEnabled = false;
			return;
		}

		const char* personaName = SteamFriends()->GetPersonaName();

		try {
			galaxy::api::User()->SignInSteam(steamAppTicket, ticketSz, personaName);
		} catch (const galaxy::api::IError &error) {
			printf("[Galaxy] Failed to begin Steam sign in: %s\n", error.GetMsg());
			g_galaxyEnabled = false;
		}
	}
};
static SteamTicketCallback g_steamTicketCallback;

int main(int argc, const char* argv[])
{
	g_ctx = new Unet::Context;
	g_ctx->SetCallbacks(new TestCallbacks);

	std::vector<s2::string> delayedCommands;

	for (int i = 1; i < argc; i++) {
		s2::string arg = argv[i];

		if (arg == "--steam" && i + 1 < argc) {
			const char* appIdStr = argv[++i];
			printf("Enabling Steam service (App ID %s)\n", appIdStr);

#if defined(_MSC_VER)
			SetEnvironmentVariableA("SteamAppID", appIdStr);
#endif
			g_steamEnabled = SteamAPI_Init();
			if (!g_steamEnabled) {
				printf("Failed to initialize Steam API!\n");
				continue;
			}

			g_ctx->EnableService(Unet::ServiceType::Steam);
			continue;
		}

		if (arg == "--galaxy" && i + 2 < argc) {
			const char* clientId = argv[++i];
			const char* clientSecret = argv[++i];
			printf("Enabling Galaxy service\n");

			g_authListener = new GalaxyAuthListener;

			try {
				galaxy::api::Init(galaxy::api::InitOptions(clientId, clientSecret));
			} catch (const galaxy::api::IError &error) {
				printf("Failed to initiailize Galaxy API: %s\n", error.GetMsg());
			}
			g_galaxyEnabled = true;

			g_ctx->EnableService(Unet::ServiceType::Galaxy);
			continue;
		}

		if (arg == "--primary" && i + 1 < argc) {
			g_ctx->SetPrimaryService(Unet::GetServiceTypeByName(argv[++i]));
			continue;
		}

		delayedCommands.emplace_back(arg);
	}

	RunCallbacks();

	if (g_galaxyEnabled) {
		if (g_steamEnabled) {
			uint32 secretData = 0;
			SteamAPICall_t hSteamAPICall = SteamUser()->RequestEncryptedAppTicket(&secretData, sizeof(secretData));
			g_steamTicketCallback.m_callback.Set(hSteamAPICall, &g_steamTicketCallback, &SteamTicketCallback::OnCallback);
		} else {
			galaxy::api::User()->SignInGalaxy(true);
		}
	}

	if (g_galaxyEnabled) {
		printf("Waiting for Galaxy sign in...\n");
		while (g_galaxyEnabled && !galaxy::api::User()->SignedIn()) {
			RunCallbacks();
		}
	}

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
