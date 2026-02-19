#include <string>
#include <vector>
#include <stdio.h>
#include <time.h>

#include "steam_api.h"

class FakeSteamUserStatsImpl
{
  public:
    SteamAPICall_t lastUploadLeaderboardScoreCall = k_uAPICallInvalid;
    SteamAPICall_t lastAttachLeaderboardUGCCall = k_uAPICallInvalid;

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
        lastUploadLeaderboardScoreCall = static_cast<SteamAPICall_t>(1);
        return lastUploadLeaderboardScoreCall;
    }

    SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t, UGCHandle_t)
    {
        lastAttachLeaderboardUGCCall = static_cast<SteamAPICall_t>(2);
        return lastAttachLeaderboardUGCCall;
    }
};

class FakeSteamRemoteStorageImpl
{
  public:
    std::vector<std::string> files;
    std::vector<std::string> deletedFiles;
    std::string lastWriteFilename;
    std::string lastShareFilename;

    SteamAPICall_t UGCDownload(UGCHandle_t, uint32)
    {
        return k_uAPICallInvalid;
    }

    int32 UGCRead(UGCHandle_t, void*, int32, uint32, EUGCReadAction)
    {
        return -1;
    }

    SteamAPICall_t FileWriteAsync(const char* name, const void*, uint32)
    {
        lastWriteFilename = name ? name : "";
        if (!lastWriteFilename.empty()) {
            bool exists = false;
            for (const auto& f : files) {
                if (f == lastWriteFilename) {
                    exists = true;
                    break;
                }
            }
            if (!exists) files.emplace_back(lastWriteFilename);
        }
        return static_cast<SteamAPICall_t>(11);
    }

    SteamAPICall_t FileShare(const char* name)
    {
        lastShareFilename = name ? name : "";
        return static_cast<SteamAPICall_t>(12);
    }

    int32 GetFileCount(void)
    {
        return static_cast<int32>(files.size());
    }

    const char* GetFileNameAndSize(int iFile, int32* pnFileSizeInBytes)
    {
        if (pnFileSizeInBytes) *pnFileSizeInBytes = 0;
        if (iFile < 0 || static_cast<size_t>(iFile) >= files.size()) return "";
        return files[static_cast<size_t>(iFile)].c_str();
    }

    bool FileDelete(const char* name)
    {
        if (!name || !name[0]) return false;
        for (auto it = files.begin(); it != files.end(); ++it) {
            if (*it == name) {
                deletedFiles.emplace_back(name);
                files.erase(it);
                return true;
            }
        }
        return false;
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

static time_t g_fakeTime = static_cast<time_t>(1700000000);
static time_t FakeTime(time_t* t)
{
    if (t) *t = g_fakeTime;
    return g_fakeTime;
}

#define STEAM_LEADERBOARD_HELPER_TIME FakeTime
#define private public
#define protected public
#include "CSteamLeaderboardHelper.hpp"
#undef protected
#undef private

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

    if (!helper.isDone()) {
        puts("FAIL: helper should become done after initialization error.");
        return 1;
    }
    if (helper.isReady()) {
        puts("FAIL: helper should not become ready after initialization error.");
        return 1;
    }
    if (!helper.hasError()) {
        puts("FAIL: helper should have error after initialization error.");
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

    // sendScore: timestamped UGC name and cleanup after attach
    {
        logs.clear();
        g_fakeRemoteStorage.files.clear();
        g_fakeRemoteStorage.deletedFiles.clear();
        g_fakeRemoteStorage.lastWriteFilename.clear();
        g_fakeRemoteStorage.lastShareFilename.clear();
        g_fakeTime = static_cast<time_t>(12345);

        CSteamLeaderboardHelper helper2("dummy_board", "lb_replay", [&](const char* msg) {
            logs.emplace_back(msg ? msg : "");
        });
        helper2.initState = CSteamLeaderboardHelper::InitState::DoneOk;
        helper2.leaderboard = static_cast<SteamLeaderboard_t>(1);

        const uint8_t data[] = {0x01, 0x02, 0x03};
        if (!helper2.sendScore(100, data, sizeof(data))) {
            puts("FAIL: sendScore should return true with valid API call handles.");
            return 1;
        }
        if (helper2.ugcUploadFilename != "dummy_board_lb_replay_12345.dat") {
            puts("FAIL: ugcUploadFilename should be timestamped with _{timestamp}.dat.");
            return 1;
        }

        LeaderboardScoreUploaded_t uploadCb{};
        uploadCb.m_bSuccess = 1;
        uploadCb.m_bScoreChanged = 1;
        helper2.onUploadScore(&uploadCb, false);

        if (g_fakeRemoteStorage.lastWriteFilename != "dummy_board_lb_replay_12345.dat") {
            puts("FAIL: FileWriteAsync should use timestamped UGC filename.");
            return 1;
        }

        RemoteStorageFileWriteAsyncComplete_t writeCb{};
        writeCb.m_eResult = k_EResultOK;
        helper2.onWriteUGC(&writeCb, false);

        if (g_fakeRemoteStorage.lastShareFilename != "dummy_board_lb_replay_12345.dat") {
            puts("FAIL: FileShare should use timestamped UGC filename.");
            return 1;
        }

        // prepare old files (should be deleted after attach)
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_1.dat");
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_2.dat");
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_abc.dat");
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_foo.txt");
        g_fakeRemoteStorage.files.emplace_back("other_99.dat");

        RemoteStorageFileShareResult_t shareCb{};
        shareCb.m_eResult = k_EResultOK;
        shareCb.m_hFile = static_cast<UGCHandle_t>(999);
        helper2.onShareUGC(&shareCb, false);

        LeaderboardUGCSet_t attachCb{};
        attachCb.m_eResult = k_EResultOK;
        helper2.onAttachUGC(&attachCb, false);

        if (helper2.isSendScoreBusy()) {
            puts("FAIL: sendScore should be idle after successful attach.");
            return 1;
        }
        bool attachedDeleted = false;
        for (const auto& f : g_fakeRemoteStorage.deletedFiles) {
            if (f == "dummy_board_lb_replay_12345.dat") attachedDeleted = true;
        }
        if (attachedDeleted) {
            puts("FAIL: cleanup should not delete the attached UGC file.");
            return 1;
        }
        bool attachedExists = false;
        for (const auto& f : g_fakeRemoteStorage.files) {
            if (f == "dummy_board_lb_replay_12345.dat") attachedExists = true;
        }
        if (!attachedExists) {
            puts("FAIL: attached UGC file should remain in remote storage.");
            return 1;
        }
        if (!logContains(logs, "Successfully attached UGC")) {
            puts("FAIL: missing log for successful UGC attach.");
            return 1;
        }
        bool hasOld1 = false;
        bool hasOld2 = false;
        for (const auto& f : g_fakeRemoteStorage.deletedFiles) {
            if (f == "dummy_board_lb_replay_1.dat") hasOld1 = true;
            if (f == "dummy_board_lb_replay_2.dat") hasOld2 = true;
            if (f == "dummy_board_lb_replay_abc.dat" || f == "dummy_board_lb_replay_foo.txt" || f == "other_99.dat") {
                puts("FAIL: cleanup should not delete non-matching files.");
                return 1;
            }
        }
	        if (!hasOld1 || !hasOld2) {
	            puts("FAIL: cleanup should delete other dummy_board_lb_replay_*.dat files.");
	            return 1;
	        }

	        // prevent duplicated timestamp for UGC sendScore
	        logs.clear();
	        if (helper2.sendScore(101, data, sizeof(data))) {
	            puts("FAIL: sendScore should be rejected when timestamp is duplicated for UGC.");
	            return 1;
	        }
	        if (helper2.isSendScoreBusy()) {
	            puts("FAIL: sendScore should remain idle when duplicated timestamp is rejected.");
	            return 1;
	        }
	        if (!logContains(logs, "duplicated UGC timestamp")) {
	            puts("FAIL: missing log for duplicated UGC timestamp.");
	            return 1;
	        }
	    }

    puts("OK");
    return 0;
}
