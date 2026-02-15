#include <string>
#include <vector>
#include <stdio.h>

#include "steam_api.h"

class FakeSteamUserStatsImpl
{
  public:
    SteamAPICall_t FindLeaderboard(const char*)
    {
        return k_uAPICallInvalid;
    }

    SteamAPICall_t DownloadLeaderboardEntries(SteamLeaderboard_t, ELeaderboardDataRequest, int, int)
    {
        return k_uAPICallInvalid;
    }

    bool GetDownloadedLeaderboardEntry(SteamLeaderboardEntries_t, int, LeaderboardEntry_t*, int32*, int)
    {
        return false;
    }

    SteamAPICall_t UploadLeaderboardScore(SteamLeaderboard_t, ELeaderboardUploadScoreMethod, int32, const int32*, int)
    {
        return k_uAPICallInvalid;
    }

    SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t, UGCHandle_t)
    {
        return k_uAPICallInvalid;
    }
};

class FakeSteamRemoteStorageImpl
{
  public:
    SteamAPICall_t UGCDownload(UGCHandle_t, uint32)
    {
        return k_uAPICallInvalid;
    }

    int32 UGCRead(UGCHandle_t, void*, int32, uint32, EUGCReadAction)
    {
        return -1;
    }

    SteamAPICall_t FileWriteAsync(const char*, const void*, uint32)
    {
        return k_uAPICallInvalid;
    }

    SteamAPICall_t FileShare(const char*)
    {
        return k_uAPICallInvalid;
    }
};

class FakeSteamFriendsImpl
{
  public:
    const char* GetPersonaName()
    {
        return "";
    }

    const char* GetFriendPersonaName(CSteamID)
    {
        return "";
    }

    bool RequestUserInformation(CSteamID, bool)
    {
        return false;
    }
};

class FakeSteamUserImpl
{
  public:
    CSteamID GetSteamID()
    {
        return CSteamID();
    }
};

static FakeSteamUserStatsImpl g_fakeUserStats;
static FakeSteamRemoteStorageImpl g_fakeRemoteStorage;
static FakeSteamFriendsImpl g_fakeFriends;
static FakeSteamUserImpl g_fakeUser;

FakeSteamUserStatsImpl* FakeSteamUserStats()
{
    return &g_fakeUserStats;
}

FakeSteamRemoteStorageImpl* FakeSteamRemoteStorage()
{
    return &g_fakeRemoteStorage;
}

FakeSteamFriendsImpl* FakeSteamFriends()
{
    return &g_fakeFriends;
}

FakeSteamUserImpl* FakeSteamUser()
{
    return &g_fakeUser;
}

#define STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS FakeSteamUserStats
#define STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE FakeSteamRemoteStorage
#define STEAM_LEADERBOARD_HELPER_STEAM_FRIENDS FakeSteamFriends
#define STEAM_LEADERBOARD_HELPER_STEAM_USER FakeSteamUser

#include "CSteamLeaderboardHelper.hpp"

static bool logContains(const std::vector<std::string>& logs, const char* needle)
{
    for (const auto& msg : logs) {
        if (msg.find(needle) != std::string::npos) return true;
    }
    return false;
}

int main()
{
    std::vector<std::string> logs;
    CSteamLeaderboardHelper helper("dummy_board", [&](const char* msg) {
        logs.emplace_back(msg ? msg : "");
    });

    helper.initialize();

    if (!helper.isReady()) {
        puts("FAIL: helper should become ready after initialization error.");
        return 1;
    }
    if (helper.canReload()) {
        puts("FAIL: helper should not be reloadable after initialization error.");
        return 1;
    }
    if (!logContains(logs, "FindLeaderboard returned invalid call handle")) {
        puts("FAIL: missing log for invalid FindLeaderboard call handle.");
        return 1;
    }

    puts("OK");
    return 0;
}
