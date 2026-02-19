#include <string>
#include <vector>
#include <unordered_map>
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
    std::vector<std::string> failDeleteFiles;
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

    bool FileExists(const char* name)
    {
        if (!name || !name[0]) return false;
        for (const auto& f : files) {
            if (f == name) return true;
        }
        return false;
    }

    bool FileDelete(const char* name)
    {
        if (!name || !name[0]) return false;
        for (const auto& f : failDeleteFiles) {
            if (f == name) return false;
        }
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
    std::unordered_map<uint64_t, std::string> friendNames;
    std::string personaName;
    int getPersonaNameCalls = 0;
    int getFriendPersonaNameCalls = 0;
    int requestUserInformationCalls = 0;
    CSteamID lastRequestedUserId{};

    const char* GetPersonaName()
    {
        getPersonaNameCalls++;
        return personaName.c_str();
    }

    bool RequestUserInformation(CSteamID userId, bool)
    {
        requestUserInformationCalls++;
        lastRequestedUserId = userId;
        return true;
    }

    const char* GetFriendPersonaName(CSteamID userId)
    {
        getFriendPersonaNameCalls++;
        const uint64_t id64 = static_cast<uint64_t>(userId.ConvertToUint64());
        auto it = friendNames.find(id64);
        if (it == friendNames.end()) return "[unknown]";
        return it->second.c_str();
    }
};

class FakeSteamUserImpl
{
  public:
    CSteamID steamId{};

    CSteamID GetSteamID()
    {
        return steamId;
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

static CSteamID makeIndividualUserId(uint32 accountId)
{
    return CSteamID(accountId, k_EUniversePublic, k_EAccountTypeIndividual);
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
        g_fakeRemoteStorage.failDeleteFiles.clear();
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
        if (!logContains(logs, "Cleanup old UGC files: candidates=2, deleted=2, already_gone=0, remaining=0")) {
            puts("FAIL: missing cleanup summary log with expected counts.");
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

    // cleanupOldUGCFiles: tolerate delete failures when file is already gone, but report remaining failures
    {
        logs.clear();
        g_fakeRemoteStorage.files.clear();
        g_fakeRemoteStorage.deletedFiles.clear();
        g_fakeRemoteStorage.failDeleteFiles.clear();
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

        LeaderboardScoreUploaded_t uploadCb{};
        uploadCb.m_bSuccess = 1;
        uploadCb.m_bScoreChanged = 1;
        helper2.onUploadScore(&uploadCb, false);

        RemoteStorageFileWriteAsyncComplete_t writeCb{};
        writeCb.m_eResult = k_EResultOK;
        helper2.onWriteUGC(&writeCb, false);

        // prepare old files and simulate a deletion failure for one of them
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_1.dat");
        g_fakeRemoteStorage.files.emplace_back("dummy_board_lb_replay_2.dat");
        g_fakeRemoteStorage.failDeleteFiles.emplace_back("dummy_board_lb_replay_2.dat");

        RemoteStorageFileShareResult_t shareCb{};
        shareCb.m_eResult = k_EResultOK;
        shareCb.m_hFile = static_cast<UGCHandle_t>(999);
        helper2.onShareUGC(&shareCb, false);

        LeaderboardUGCSet_t attachCb{};
        attachCb.m_eResult = k_EResultOK;
        helper2.onAttachUGC(&attachCb, false);

        if (!logContains(logs, "Cleanup old UGC files: candidates=2, deleted=1, already_gone=0, remaining=1")) {
            puts("FAIL: missing cleanup summary log with remaining failures.");
            return 1;
        }
        bool remainingExists = false;
        for (const auto& f : g_fakeRemoteStorage.files) {
            if (f == "dummy_board_lb_replay_2.dat") remainingExists = true;
        }
        if (!remainingExists) {
            puts("FAIL: failed delete file should remain in remote storage.");
            return 1;
        }
    }

    // downloadUGC: validate args/handle before setting busy state
    {
        logs.clear();
        CSteamLeaderboardHelper helper2("dummy_board", [&](const char* msg) {
            logs.emplace_back(msg ? msg : "");
        });

        bool cbCalled = false;
        bool busyDuringCb = true;
        bool cbBadData = false;
        helper2.downloadUGC(nullptr, [&](const uint8_t* data, size_t size) {
            cbCalled = true;
            busyDuringCb = helper2.isDownloadBusyUGC();
            if (data != nullptr || size != 0) cbBadData = true;
        });

        if (!cbCalled) {
            puts("FAIL: downloadUGC should call callback on invalid entry.");
            return 1;
        }
        if (cbBadData) {
            puts("FAIL: downloadUGC should pass (nullptr, 0) on invalid entry.");
            return 1;
        }
        if (busyDuringCb || helper2.isDownloadBusyUGC()) {
            puts("FAIL: downloadUGC should not become busy on invalid entry.");
            return 1;
        }
        if (!logContains(logs, "UGC download failed: invalid entry")) {
            puts("FAIL: missing log for invalid entry in downloadUGC.");
            return 1;
        }

        logs.clear();
        LeaderboardEntry_t e{};
        e.m_hUGC = static_cast<UGCHandle_t>(123);
        e.m_nGlobalRank = 1;

        cbCalled = false;
        busyDuringCb = true;
        cbBadData = false;
        helper2.downloadUGC(&e, [&](const uint8_t* data, size_t size) {
            cbCalled = true;
            busyDuringCb = helper2.isDownloadBusyUGC();
            if (data != nullptr || size != 0) cbBadData = true;
        });

        if (!cbCalled) {
            puts("FAIL: downloadUGC should call callback when UGCDownload returns invalid handle.");
            return 1;
        }
        if (cbBadData) {
            puts("FAIL: downloadUGC should pass (nullptr, 0) on invalid call handle.");
            return 1;
        }
        if (busyDuringCb || helper2.isDownloadBusyUGC()) {
            puts("FAIL: downloadUGC should not become busy when UGCDownload fails synchronously.");
            return 1;
        }
        if (!logContains(logs, "UGCDownload returned invalid call handle")) {
            puts("FAIL: missing log for invalid UGCDownload call handle.");
            return 1;
        }

        logs.clear();
        std::function<void(const uint8_t*, size_t)> nullCb;
        helper2.downloadUGC(&e, nullCb);
        if (helper2.isDownloadBusyUGC()) {
            puts("FAIL: downloadUGC should remain idle when callback is null.");
            return 1;
        }
        if (!logContains(logs, "UGC download failed: callback is null")) {
            puts("FAIL: missing log for null callback in downloadUGC.");
            return 1;
        }
    }

    // finishSendScore: defer reload while UGC download is in-flight
    {
        logs.clear();
        CSteamLeaderboardHelper helper3("dummy_board", [&](const char* msg) {
            logs.emplace_back(msg ? msg : "");
        });
        helper3.initState = CSteamLeaderboardHelper::InitState::DoneOk;
        helper3.leaderboard = static_cast<SteamLeaderboard_t>(1);
        helper3.sendScoreState = CSteamLeaderboardHelper::SendScoreState::UploadingScore;

        bool ugcCbCalled = false;
        bool ugcCbBadData = false;
        helper3.ugcDownloadCallback = [&](const uint8_t* data, size_t size) {
            ugcCbCalled = true;
            if (data != nullptr || size != 0) {
                ugcCbBadData = true;
            }
        };

        helper3.finishSendScore(true);

        if (!helper3.reloadDeferred) {
            puts("FAIL: reload should be deferred while UGC download is in progress.");
            return 1;
        }
        if (helper3.downloadTopState != CSteamLeaderboardHelper::DownloadState::Idle ||
            helper3.downloadMineState != CSteamLeaderboardHelper::DownloadState::Idle) {
            puts("FAIL: reload should not start while UGC download is in progress.");
            return 1;
        }
        if (!logContains(logs, "Reload deferred: UGC download is still in progress")) {
            puts("FAIL: missing log for deferred reload during UGC download.");
            return 1;
        }
        if (logContains(logs, "Reloading leaderboard")) {
            puts("FAIL: reload should not be executed immediately when deferred.");
            return 1;
        }

        RemoteStorageDownloadUGCResult_t ugcCb{};
        ugcCb.m_eResult = k_EResultOK;
        ugcCb.m_nSizeInBytes = 10;
        ugcCb.m_hFile = static_cast<UGCHandle_t>(555);
        helper3.onDownloadUGC(&ugcCb, false);

        if (!ugcCbCalled) {
            puts("FAIL: UGC callback should be called on UGC read failure.");
            return 1;
        }
        if (ugcCbBadData) {
            puts("FAIL: UGC callback should get (nullptr, 0) on read failure.");
            return 1;
        }
        if (helper3.reloadDeferred) {
            puts("FAIL: deferred reload flag should be cleared after UGC download completes.");
            return 1;
        }
        if (helper3.downloadTopState == CSteamLeaderboardHelper::DownloadState::Idle ||
            helper3.downloadMineState == CSteamLeaderboardHelper::DownloadState::Idle) {
            puts("FAIL: deferred reload should be attempted after UGC download completes.");
            return 1;
        }
        if (!logContains(logs, "Reloading leaderboard")) {
            puts("FAIL: missing log for reload attempt after UGC download completes.");
            return 1;
        }
    }

    // getUserName: cache up to maxEntries+1 (FIFO)
    {
        g_fakeFriends.friendNames.clear();
        g_fakeFriends.personaName.clear();
        g_fakeFriends.getPersonaNameCalls = 0;
        g_fakeFriends.getFriendPersonaNameCalls = 0;
        g_fakeFriends.requestUserInformationCalls = 0;
        g_fakeFriends.lastRequestedUserId = CSteamID();
        g_fakeUser.steamId = CSteamID();

        CSteamLeaderboardHelper helper4("dummy_board", [&](const char*) {});
        helper4.setMaxEntries(2); // cache limit = maxEntries + 1 = 3

        const CSteamID id1 = makeIndividualUserId(1);
        const CSteamID id2 = makeIndividualUserId(2);
        const CSteamID id3 = makeIndividualUserId(3);
        const CSteamID id4 = makeIndividualUserId(4);
        const CSteamID id5 = makeIndividualUserId(5);

        g_fakeFriends.friendNames[static_cast<uint64_t>(id1.ConvertToUint64())] = "Alice";
        g_fakeFriends.friendNames[static_cast<uint64_t>(id2.ConvertToUint64())] = "Bob";
        g_fakeFriends.friendNames[static_cast<uint64_t>(id3.ConvertToUint64())] = "Carol";
        g_fakeFriends.friendNames[static_cast<uint64_t>(id4.ConvertToUint64())] = "Dave";
        g_fakeFriends.friendNames[static_cast<uint64_t>(id5.ConvertToUint64())] = "Eve";

        LeaderboardEntry_t e1{};
        e1.m_steamIDUser = id1;
        LeaderboardEntry_t e2{};
        e2.m_steamIDUser = id2;
        LeaderboardEntry_t e3{};
        e3.m_steamIDUser = id3;
        LeaderboardEntry_t e4{};
        e4.m_steamIDUser = id4;
        LeaderboardEntry_t e5{};
        e5.m_steamIDUser = id5;

        const char* n1 = helper4.getUserName(&e1);
        const char* n2 = helper4.getUserName(&e2);
        const char* n3 = helper4.getUserName(&e3);
        if (!n1 || std::string(n1) != "Alice" || !n2 || std::string(n2) != "Bob" || !n3 || std::string(n3) != "Carol") {
            puts("FAIL: getUserName should return cached friend persona names.");
            return 1;
        }
        if (g_fakeFriends.getFriendPersonaNameCalls != 3) {
            puts("FAIL: getUserName should call GetFriendPersonaName once per uncached user.");
            return 1;
        }

        // cache hit should not call GetFriendPersonaName again and should not affect FIFO order
        const char* n1b = helper4.getUserName(&e1);
        if (!n1b || std::string(n1b) != "Alice") {
            puts("FAIL: getUserName should return cached name on hit.");
            return 1;
        }
        if (g_fakeFriends.getFriendPersonaNameCalls != 3) {
            puts("FAIL: getUserName cache hit should not call GetFriendPersonaName.");
            return 1;
        }

        // Insert 4th distinct user => evict the oldest (id1). Cache size remains 3.
        const char* n4 = helper4.getUserName(&e4);
        if (!n4 || std::string(n4) != "Dave") {
            puts("FAIL: getUserName should return friend persona name for newly inserted user.");
            return 1;
        }
        if (helper4.userNameCache.size() != 3) {
            puts("FAIL: userNameCache should be capped to maxEntries+1.");
            return 1;
        }
        if (helper4.userNameCache.find(static_cast<uint64_t>(id1.ConvertToUint64())) != helper4.userNameCache.end()) {
            puts("FAIL: FIFO cache should evict the oldest entry (id1).");
            return 1;
        }

        // Access id2 (hit), then insert id5 => FIFO should evict id2 (not LRU).
        const char* n2b = helper4.getUserName(&e2);
        if (!n2b || std::string(n2b) != "Bob") {
            puts("FAIL: getUserName should return cached name for id2.");
            return 1;
        }
        const char* n5 = helper4.getUserName(&e5);
        if (!n5 || std::string(n5) != "Eve") {
            puts("FAIL: getUserName should return friend persona name for id5.");
            return 1;
        }
        if (helper4.userNameCache.find(static_cast<uint64_t>(id2.ConvertToUint64())) != helper4.userNameCache.end()) {
            puts("FAIL: FIFO cache eviction should not be affected by cache hits (id2 should be evicted).");
            return 1;
        }
        if (helper4.userNameCache.find(static_cast<uint64_t>(id3.ConvertToUint64())) == helper4.userNameCache.end()) {
            puts("FAIL: FIFO cache should retain newer entries (id3 should remain).");
            return 1;
        }
    }

    // getUserName: unknown names should request user information and should not be cached
    {
        g_fakeFriends.friendNames.clear();
        g_fakeFriends.personaName.clear();
        g_fakeFriends.getPersonaNameCalls = 0;
        g_fakeFriends.getFriendPersonaNameCalls = 0;
        g_fakeFriends.requestUserInformationCalls = 0;
        g_fakeFriends.lastRequestedUserId = CSteamID();
        g_fakeUser.steamId = CSteamID();

        CSteamLeaderboardHelper helper5("dummy_board", [&](const char*) {});
        helper5.setMaxEntries(1);

        const CSteamID unknownId = makeIndividualUserId(42);
        LeaderboardEntry_t e{};
        e.m_steamIDUser = unknownId;
        const char* name = helper5.getUserName(&e);
        if (name != nullptr) {
            puts("FAIL: getUserName should return nullptr for unknown name.");
            return 1;
        }
        if (g_fakeFriends.requestUserInformationCalls != 1) {
            puts("FAIL: getUserName should request user information for unknown name.");
            return 1;
        }
        if (helper5.userNameCache.find(static_cast<uint64_t>(unknownId.ConvertToUint64())) != helper5.userNameCache.end()) {
            puts("FAIL: getUserName should not cache unknown name results.");
            return 1;
        }
    }

    // getUserName: current user name should be cached too
    {
        g_fakeFriends.friendNames.clear();
        g_fakeFriends.personaName = "Me";
        g_fakeFriends.getPersonaNameCalls = 0;
        g_fakeFriends.getFriendPersonaNameCalls = 0;
        g_fakeFriends.requestUserInformationCalls = 0;
        g_fakeFriends.lastRequestedUserId = CSteamID();

        const CSteamID myId = makeIndividualUserId(7);
        g_fakeUser.steamId = myId;

        CSteamLeaderboardHelper helper6("dummy_board", [&](const char*) {});
        helper6.setMaxEntries(1);

        LeaderboardEntry_t e{};
        e.m_steamIDUser = myId;
        const char* name1 = helper6.getUserName(&e);
        const char* name2 = helper6.getUserName(&e);
        if (!name1 || std::string(name1) != "Me" || !name2 || std::string(name2) != "Me") {
            puts("FAIL: getUserName should return current user persona name.");
            return 1;
        }
        if (g_fakeFriends.getPersonaNameCalls != 1) {
            puts("FAIL: current user name should be served from cache after first call.");
            return 1;
        }
    }

    puts("OK");
    return 0;
}
