/**
 * Steam Leaderboard Helper
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Yoji Suzuki.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once
#include <vector>
#include <string>
#include <functional>
#include <utility>
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <cctype>
#include <time.h>
#include "steam_api.h"

#ifndef STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS
#define STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS SteamUserStats
#endif
#ifndef STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE
#define STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE SteamRemoteStorage
#endif
#ifndef STEAM_LEADERBOARD_HELPER_STEAM_FRIENDS
#define STEAM_LEADERBOARD_HELPER_STEAM_FRIENDS SteamFriends
#endif
#ifndef STEAM_LEADERBOARD_HELPER_STEAM_USER
#define STEAM_LEADERBOARD_HELPER_STEAM_USER SteamUser
#endif
#ifndef STEAM_LEADERBOARD_HELPER_TIME
#define STEAM_LEADERBOARD_HELPER_TIME time
#endif

class CSteamLeaderboardHelper
{
  private:
    enum class InitState {
        Idle,
        InProgress,
        DoneOk,
        DoneError,
    };

    enum class DownloadState {
        Idle,
        InProgress,
        DoneOk,
        DoneError,
    };

    enum class SendScoreState {
        Idle,
        UploadingScore,
        WritingUGC,
        SharingUGC,
        AttachingUGC,
    };

    CCallResult<CSteamLeaderboardHelper, LeaderboardFindResult_t> callResultFindLeaderboard;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoresDownloaded_t> callResultDownloadLeaderboardScoreTop;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoresDownloaded_t> callResultDownloadLeaderboardScoreMine;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoreUploaded_t> callResultUploadLeaderboardScore;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageFileWriteAsyncComplete_t> callResultWriteUGC;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageFileShareResult_t> callResultShareUGC;
    CCallResult<CSteamLeaderboardHelper, LeaderboardUGCSet_t> callResultAttachUGC;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageDownloadUGCResult_t> callResultDownloadUGC;
    std::vector<uint8_t> ugcUploadData;
    std::vector<uint8_t> ugcDownloadData;
    std::string boardName;
    std::string ugcName;
    std::string ugcUploadFilename;
    long long lastUGCSendScoreTimestamp;
    InitState initState;
    int maxEntries;
    SteamLeaderboard_t leaderboard;
    SendScoreState sendScoreState;
    std::function<void(const char*)> logger;
    std::function<void(const uint8_t* data, size_t size)> ugcDownloadCallback;
    bool reloadDeferred;

    std::vector<LeaderboardEntry_t> top;
    bool topRanksDownloaded;
    LeaderboardEntry_t myRank;
    bool myRankDownloaded;
    DownloadState downloadTopState;
    DownloadState downloadMineState;

    void finishSendScore(bool shouldReload)
    {
        sendScoreState = SendScoreState::Idle;
        ugcUploadData.clear();
        ugcUploadFilename.clear();
        if (shouldReload) {
            if (ugcDownloadCallback) {
                putlog("Reload deferred: UGC download is still in progress (%s).", boardName.c_str());
                reloadDeferred = true;
            } else {
                this->reload();
            }
        }
    }

    void maybeReloadDeferred(void)
    {
        if (!reloadDeferred) return;
        if (ugcDownloadCallback) return;
        if (initState != InitState::DoneOk || 0 == leaderboard) return;
        if (downloadTopState == DownloadState::InProgress || downloadMineState == DownloadState::InProgress) return;
        reloadDeferred = false;
        this->reload();
    }

    void putlog(const char* msg, ...)
    {
        if (!logger) return;
        char buf[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buf, sizeof(buf), msg, args);
        va_end(args);
        logger(buf);
    }

  public:
    /**
     * @brief Constructor without a UGC filename
     * @param boardName Leaderboard name
     * @param logger Log callback
     */
    CSteamLeaderboardHelper(std::string boardName, std::function<void(const char*)> logger)
        : CSteamLeaderboardHelper(std::move(boardName), "replay", std::move(logger))
    {
    }

    /**
     * @brief Constructor
     * @param boardName Leaderboard name
     * @param ugcBaseName UGC filename on Steam Cloud (base name)
     * @param logger Log callback
     */
    CSteamLeaderboardHelper(std::string boardName, std::string ugcBaseName, std::function<void(const char*)> logger)
        : leaderboard(0),
          boardName(std::move(boardName)),
          lastUGCSendScoreTimestamp(-1),
          initState(InitState::Idle),
          sendScoreState(SendScoreState::Idle),
          reloadDeferred(false),
          topRanksDownloaded(false),
          myRankDownloaded(false),
          downloadTopState(DownloadState::Idle),
          downloadMineState(DownloadState::Idle)
    {
        this->logger = std::move(logger);

        // sanitize boardName for filename
        std::string safeBoard = this->boardName;
        std::replace_if(safeBoard.begin(), safeBoard.end(), [](char c) { return !std::isalnum(static_cast<unsigned char>(c)) && c != '_'; }, '_');
        this->ugcName = safeBoard + "_" + ugcBaseName;

        ugcUploadData.clear();
        ugcDownloadData.clear();
        setMaxEntries(100);
    }

    ~CSteamLeaderboardHelper()
    {
        callResultFindLeaderboard.Cancel();
        callResultDownloadLeaderboardScoreTop.Cancel();
        callResultDownloadLeaderboardScoreMine.Cancel();
        callResultUploadLeaderboardScore.Cancel();
        callResultWriteUGC.Cancel();
        callResultShareUGC.Cancel();
        callResultAttachUGC.Cancel();
        callResultDownloadUGC.Cancel();
    }

    /**
     * @brief set max entry number
     * @param maxEntries max entry number
     */
    void setMaxEntries(int maxEntries)
    {
        this->maxEntries = maxEntries < 1 ? 1 : maxEntries;
    }

    /**
     * @brief Returns whether a score submission request is in progress
     * @return true: busy, false: idle
     */
    bool isSendScoreBusy(void) const
    {
        return sendScoreState != SendScoreState::Idle;
    }

    /**
     * @brief Returns whether a reload request is in progress
     * @return true: busy, false: idle
     */
    bool isReloadBusy(void) const
    {
        return downloadTopState == DownloadState::InProgress || downloadMineState == DownloadState::InProgress;
    }

    /**
     * @brief Returns whether reload() can be called now
     * @return true: can call reload, false: cannot
     */
    bool canReload(void) const
    {
        return initState == InitState::DoneOk && 0 != leaderboard && !isReloadBusy();
    }

    /**
     * @brief Initializes the helper
     * @remark Automatically reloads entries after initialization.
     */
    void initialize(void)
    {
        if (initState == InitState::InProgress || initState == InitState::DoneOk) return;
        callResultFindLeaderboard.Cancel();
        leaderboard = 0;
        topRanksDownloaded = false;
        myRankDownloaded = false;
        downloadTopState = DownloadState::Idle;
        downloadMineState = DownloadState::Idle;

        putlog("Initializing Steam leaderboard: %s", boardName.c_str());
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Initialize failed: SteamUserStats is not available (%s).", boardName.c_str());
            initState = InitState::DoneError;
            return;
        }
        auto hdl = stats->FindLeaderboard(boardName.c_str());
        if (k_uAPICallInvalid == hdl) {
            putlog("Initialize failed: FindLeaderboard returned invalid call handle (%s).", boardName.c_str());
            initState = InitState::DoneError;
            return;
        }
        initState = InitState::InProgress;
        this->callResultFindLeaderboard.Set(hdl, this, &CSteamLeaderboardHelper::onFindLeaderboard);
    }

    /**
     * @brief Check ready
     * @return true: ready, false: not ready
     */
    bool isReady(void) const
    {
        if (initState != InitState::DoneOk) {
            return false;
        }
        return downloadTopState == DownloadState::DoneOk && downloadMineState == DownloadState::DoneOk;
    }

    /**
     * @brief Check done (success or failure)
     * @return true: done, false: in progress
     */
    bool isDone(void) const
    {
        if (initState == InitState::DoneError) {
            return true;
        }
        if (initState != InitState::DoneOk) {
            return false;
        }
        if (downloadTopState == DownloadState::Idle || downloadTopState == DownloadState::InProgress) {
            return false;
        }
        if (downloadMineState == DownloadState::Idle || downloadMineState == DownloadState::InProgress) {
            return false;
        }
        return true;
    }

    /**
     * @brief Check error state
     * @return true: error, false: no error or not finished yet
     */
    bool hasError(void) const
    {
        if (initState == InitState::DoneError) {
            return true;
        }
        if (initState != InitState::DoneOk) {
            return false;
        }
        return downloadTopState == DownloadState::DoneError || downloadMineState == DownloadState::DoneError;
    }

    /**
     * @brief Reloads entries (top ranks and current user)
     * @return true: success, false: failed
     */
    bool reload(void)
    {
        if (initState != InitState::DoneOk || 0 == leaderboard) {
            putlog("Reload failed: leaderboard is not initialized (%s).", boardName.c_str());
            return false;
        }
        if (isReloadBusy()) {
            putlog("Reload failed: another reload request is still in progress (%s).", boardName.c_str());
            return false;
        }
        // A reload request is about to be started (or attempted). Clear any deferred reload flag.
        reloadDeferred = false;
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Reload failed: SteamUserStats is not available (%s).", boardName.c_str());
            downloadTopState = DownloadState::DoneError;
            downloadMineState = DownloadState::DoneError;
            return false;
        }
        putlog("Reloading leaderboard: %s", boardName.c_str());
        // top ranks
        topRanksDownloaded = false;
        downloadTopState = DownloadState::InProgress;
        auto handleRanking = stats->DownloadLeaderboardEntries(this->leaderboard, k_ELeaderboardDataRequestGlobal, 1, maxEntries);
        if (k_uAPICallInvalid == handleRanking) {
            putlog("Reload failed: DownloadLeaderboardEntries (top ranks) returned invalid call handle (%s).", boardName.c_str());
            downloadTopState = DownloadState::DoneError;
        } else {
            this->callResultDownloadLeaderboardScoreTop.Set(handleRanking, this, &CSteamLeaderboardHelper::onDownloadLeaderboardScoreTop);
        }
        // current user
        // AroundUser: rangeStart=0, rangeEnd=0 => only current user entry
        myRankDownloaded = false;
        downloadMineState = DownloadState::InProgress;
        auto handleMine = stats->DownloadLeaderboardEntries(this->leaderboard, k_ELeaderboardDataRequestGlobalAroundUser, 0, 0);
        if (k_uAPICallInvalid == handleMine) {
            putlog("Reload failed: DownloadLeaderboardEntries (current user) returned invalid call handle (%s).", boardName.c_str());
            downloadMineState = DownloadState::DoneError;
        } else {
            this->callResultDownloadLeaderboardScoreMine.Set(handleMine, this, &CSteamLeaderboardHelper::onDownloadLeaderboardScoreMine);
        }
        return downloadTopState != DownloadState::DoneError && downloadMineState != DownloadState::DoneError;
    }

    /**
     * @brief Returns an entry from the cached top ranks
     * @param index Entry index (0-based)
     * @return Non-null: entry, null: not available
     */
    LeaderboardEntry_t* getEntry(int index)
    {
        if (!topRanksDownloaded || index < 0 || static_cast<size_t>(index) >= top.size()) {
            return nullptr;
        }
        return &top[index];
    }

    /**
     * @brief Returns the current user's cached entry
     * @return Non-null: entry, null: not available
     */
    LeaderboardEntry_t* getMyEntry(void)
    {
        if (!myRankDownloaded || 0 == myRank.m_nGlobalRank) {
            return nullptr;
        }
        return &myRank;
    }

    /**
     * @brief Returns the Steam account user's name
     * @return Non-null: entry, null: not available
     */
    const char* getUserName(LeaderboardEntry_t* entry)
    {
        if (!entry) return nullptr;
        auto friends = STEAM_LEADERBOARD_HELPER_STEAM_FRIENDS();
        if (!friends) return nullptr;
        if (!entry->m_steamIDUser.IsValid()) return nullptr;

        auto user = STEAM_LEADERBOARD_HELPER_STEAM_USER();
        if (user && user->GetSteamID() == entry->m_steamIDUser) {
            const char* name = friends->GetPersonaName();
            return (name && name[0]) ? name : nullptr;
        }

        const char* name = friends->GetFriendPersonaName(entry->m_steamIDUser);
        if (name && name[0] && 0 != strcmp(name, "[unknown]")) {
            return name;
        }
        friends->RequestUserInformation(entry->m_steamIDUser, true);
        return nullptr;
    }

    /**
     * @brief Downloads UGC data attached to an entry
     * @param entry Target entry
     * @param callback Called with (data, size) when finished. On failure, (nullptr, 0).
     */
    void downloadUGC(LeaderboardEntry_t* entry, std::function<void(const uint8_t* data, size_t size)> callback)
    {
        if (ugcDownloadCallback) {
            putlog("UGC download failed: another downloadUGC request is still in progress (%s).", boardName.c_str());
            if (callback) callback(nullptr, 0);
            return;
        }
        ugcDownloadCallback = std::move(callback);
        ugcDownloadData.clear();
        if (!entry || 0 == entry->m_hUGC) {
            putlog("UGC download failed: invalid entry.");
            if (ugcDownloadCallback) ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            return;
        }
        auto storage = STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE();
        if (!storage) {
            putlog("UGC download failed: SteamRemoteStorage is not available (%s).", boardName.c_str());
            if (ugcDownloadCallback) ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            return;
        }
        putlog("Downloading UGC for rank #%d on leaderboard %s.", entry->m_nGlobalRank, boardName.c_str());
        auto hdl = storage->UGCDownload(entry->m_hUGC, 0);
        if (k_uAPICallInvalid == hdl) {
            putlog("UGC download failed: UGCDownload returned invalid call handle (%s).", boardName.c_str());
            if (ugcDownloadCallback) ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            return;
        }
        this->callResultDownloadUGC.Set(hdl, this, &CSteamLeaderboardHelper::onDownloadUGC);
    }

    /**
     * @brief check downloading UGC
     * @return true: busy, false: idle
     */
    bool isDownloadBusyUGC(void)
    {
        return ugcDownloadCallback ? true : false;
    }

    /**
     * @brief Uploads a score without UGC
     * @param score Score
     * @return true: success, false: failed
     */
    bool sendScore(int score)
    {
        return sendScore(score, nullptr, 0);
    }

    /**
     * @brief Uploads a score with UGC
     * @param score Score
     * @param data UGC data (optional)
     * @param size UGC data size in bytes
     * @return true: success, false: failed
     */
    bool sendScore(int score, const uint8_t* data, size_t size)
    {
        if (initState != InitState::DoneOk || 0 == leaderboard) {
            putlog("Upload failed: leaderboard is not initialized (%s).", boardName.c_str());
            return false;
        }
        if (isSendScoreBusy()) {
            putlog("Upload failed: another sendScore request is still in progress (%s).", boardName.c_str());
            return false;
        }
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Upload failed: SteamUserStats is not available (%s).", boardName.c_str());
            return false;
        }
        if (data && 0 < size) {
            ugcUploadData.assign(data, data + size);
            const long long timestamp = static_cast<long long>(STEAM_LEADERBOARD_HELPER_TIME(nullptr));
            if (timestamp == lastUGCSendScoreTimestamp) {
                putlog("Upload failed: duplicated UGC timestamp (%lld) (%s).", timestamp, boardName.c_str());
                ugcUploadData.clear();
                ugcUploadFilename.clear();
                return false;
            }
            lastUGCSendScoreTimestamp = timestamp;
            ugcUploadFilename = ugcName + "_" + std::to_string(timestamp) + ".dat";
        } else {
            ugcUploadData.clear();
            ugcUploadFilename.clear();
        }
        sendScoreState = SendScoreState::UploadingScore;
        auto hdl = stats->UploadLeaderboardScore(this->leaderboard, k_ELeaderboardUploadScoreMethodKeepBest, score, nullptr, 0);
        if (k_uAPICallInvalid == hdl) {
            putlog("Upload failed: UploadLeaderboardScore returned invalid call handle (%s).", boardName.c_str());
            finishSendScore(false);
            return false;
        }
        this->callResultUploadLeaderboardScore.Set(hdl, this, &CSteamLeaderboardHelper::onUploadScore);
        return true;
    }

  private:
    void onFindLeaderboard(LeaderboardFindResult_t* callback, bool failed)
    {
        if (failed || !callback || !callback->m_bLeaderboardFound) {
            putlog("Leaderboard not found or request failed: %s", boardName.c_str());
            initState = InitState::DoneError;
            return;
        }
        initState = InitState::DoneOk;
        putlog("Leaderboard found: %s", boardName.c_str());
        this->leaderboard = callback->m_hSteamLeaderboard;
        this->reload();
    }

    void onDownloadLeaderboardScoreTop(LeaderboardScoresDownloaded_t* callback, bool failed)
    {
        if (failed || !callback) {
            putlog("Failed to download leaderboard entries: %s", boardName.c_str());
            downloadTopState = DownloadState::DoneError;
            maybeReloadDeferred();
            return;
        }
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Failed to download leaderboard entries: SteamUserStats is not available (%s).", boardName.c_str());
            downloadTopState = DownloadState::DoneError;
            maybeReloadDeferred();
            return;
        }
        putlog("Downloaded %d entries from leaderboard %s.", callback->m_cEntryCount, boardName.c_str());
        top.resize(static_cast<size_t>(callback->m_cEntryCount));
        for (int i = 0; i < callback->m_cEntryCount; i++) {
            stats->GetDownloadedLeaderboardEntry(
                callback->m_hSteamLeaderboardEntries,
                i,
                &top[static_cast<size_t>(i)],
                nullptr,
                0);
        }
        topRanksDownloaded = true;
        downloadTopState = DownloadState::DoneOk;
        maybeReloadDeferred();
    }

    void onDownloadLeaderboardScoreMine(LeaderboardScoresDownloaded_t* callback, bool failed)
    {
        if (failed || !callback) {
            putlog("Failed to download current user's leaderboard entry: %s", boardName.c_str());
            downloadMineState = DownloadState::DoneError;
            maybeReloadDeferred();
            return;
        }
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Failed to download current user's leaderboard entry: SteamUserStats is not available (%s).", boardName.c_str());
            downloadMineState = DownloadState::DoneError;
            maybeReloadDeferred();
            return;
        }
        if (0 == callback->m_cEntryCount) {
            putlog("No entry for the current user on leaderboard %s.", boardName.c_str());
            myRank = LeaderboardEntry_t{};
        } else {
            stats->GetDownloadedLeaderboardEntry(callback->m_hSteamLeaderboardEntries, 0, &myRank, nullptr, 0);
        }
        myRankDownloaded = true;
        downloadMineState = DownloadState::DoneOk;
        maybeReloadDeferred();
    }

    void onDownloadUGC(RemoteStorageDownloadUGCResult_t* callback, bool failed)
    {
        if (!ugcDownloadCallback) return;
        if (failed || !callback || callback->m_eResult != k_EResultOK || callback->m_nSizeInBytes <= 0) {
            putlog("UGC download failed for leaderboard %s (result=%d).", boardName.c_str(), callback ? callback->m_eResult : -1);
            ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            ugcDownloadData.clear();
            maybeReloadDeferred();
            return;
        }
        auto storage = STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE();
        if (!storage) {
            putlog("UGC download failed: SteamRemoteStorage is not available (%s).", boardName.c_str());
            ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            ugcDownloadData.clear();
            maybeReloadDeferred();
            return;
        }
        ugcDownloadData.resize(static_cast<size_t>(callback->m_nSizeInBytes));
        const int32 bytesRead = storage->UGCRead(
            callback->m_hFile,
            ugcDownloadData.data(),
            callback->m_nSizeInBytes,
            0,
            k_EUGCRead_ContinueReadingUntilFinished);
        if (bytesRead <= 0) {
            putlog("UGC read failed on leaderboard %s.", boardName.c_str());
            ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            ugcDownloadData.clear();
            maybeReloadDeferred();
            return;
        }
        ugcDownloadData.resize(static_cast<size_t>(bytesRead));
        ugcDownloadCallback(ugcDownloadData.data(), ugcDownloadData.size());
        ugcDownloadCallback = nullptr;
        maybeReloadDeferred();
    }

    void onUploadScore(LeaderboardScoreUploaded_t* callback, bool failed)
    {
        if (failed || !callback || !callback->m_bSuccess) {
            putlog("Failed to upload score to leaderboard %s.", boardName.c_str());
            finishSendScore(false);
            return;
        }
        if (!callback->m_bScoreChanged) {
            putlog("High score unchanged for leaderboard %s.", boardName.c_str());
            finishSendScore(false);
            return;
        }
        if (ugcUploadData.empty()) {
            putlog("Score uploaded to leaderboard %s (no UGC attached).", boardName.c_str());
            finishSendScore(true);
            return;
        }
        if (ugcUploadFilename.empty()) {
            const long long timestamp = static_cast<long long>(STEAM_LEADERBOARD_HELPER_TIME(nullptr));
            lastUGCSendScoreTimestamp = timestamp;
            ugcUploadFilename = ugcName + "_" + std::to_string(timestamp) + ".dat";
        }
        auto storage = STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE();
        if (!storage) {
            putlog("Failed to upload UGC: SteamRemoteStorage is not available (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        putlog("Writing UGC to Steam Cloud: %s", ugcUploadFilename.c_str());
        sendScoreState = SendScoreState::WritingUGC;
        auto hdl = storage->FileWriteAsync(ugcUploadFilename.c_str(), ugcUploadData.data(), ugcUploadData.size());
        if (k_uAPICallInvalid == hdl) {
            putlog("Failed to write UGC to Steam Cloud: invalid call handle (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        this->callResultWriteUGC.Set(hdl, this, &CSteamLeaderboardHelper::onWriteUGC);
    }

    void onWriteUGC(RemoteStorageFileWriteAsyncComplete_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to write UGC to Steam Cloud (result=%d).", callback ? callback->m_eResult : -1);
            finishSendScore(false);
            return;
        }
        auto storage = STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE();
        if (!storage) {
            putlog("Failed to share UGC: SteamRemoteStorage is not available (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        putlog("Sharing UGC in Steam Cloud: %s", ugcUploadFilename.c_str());
        sendScoreState = SendScoreState::SharingUGC;
        auto hdl = storage->FileShare(ugcUploadFilename.c_str());
        if (k_uAPICallInvalid == hdl) {
            putlog("Failed to share UGC in Steam Cloud: invalid call handle (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        this->callResultShareUGC.Set(hdl, this, &CSteamLeaderboardHelper::onShareUGC);
    }

    void onShareUGC(RemoteStorageFileShareResult_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to share UGC in Steam Cloud (result=%d).", callback ? callback->m_eResult : -1);
            finishSendScore(false);
            return;
        }
        auto stats = STEAM_LEADERBOARD_HELPER_STEAM_USER_STATS();
        if (!stats) {
            putlog("Failed to attach UGC: SteamUserStats is not available (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        putlog("Attaching UGC to leaderboard %s.", boardName.c_str());
        sendScoreState = SendScoreState::AttachingUGC;
        auto hdl = stats->AttachLeaderboardUGC(leaderboard, callback->m_hFile);
        if (k_uAPICallInvalid == hdl) {
            putlog("Failed to attach UGC to the leaderboard: invalid call handle (%s).", boardName.c_str());
            finishSendScore(false);
            return;
        }
        this->callResultAttachUGC.Set(hdl, this, &CSteamLeaderboardHelper::onAttachUGC);
    }

    void onAttachUGC(LeaderboardUGCSet_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to attach UGC to the leaderboard (result=%d).", callback ? callback->m_eResult : -1);
            finishSendScore(false);
            return;
        }
        putlog("Successfully attached UGC to leaderboard %s.", boardName.c_str());
        cleanupOldUGCFiles();
        finishSendScore(true);
    }

    void cleanupOldUGCFiles(void)
    {
        if (ugcUploadFilename.empty()) return;
        auto storage = STEAM_LEADERBOARD_HELPER_STEAM_REMOTE_STORAGE();
        if (!storage) {
            putlog("Cleanup skipped: SteamRemoteStorage is not available (%s).", boardName.c_str());
            return;
        }
        const std::string prefix = ugcName + "_";
        const std::string suffix = ".dat";
        const int count = storage->GetFileCount();
        if (count <= 0) return;
        std::vector<std::string> toDelete;
        toDelete.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; i++) {
            int32 dummySize = 0;
            const char* name = storage->GetFileNameAndSize(i, &dummySize);
            if (!name || !name[0]) continue;
            std::string filename(name);
            if (filename == ugcUploadFilename) continue;
            if (0 != filename.rfind(prefix, 0)) continue;
            if (filename.size() < prefix.size() + suffix.size()) continue;
            if (0 != filename.compare(filename.size() - suffix.size(), suffix.size(), suffix)) continue;
            const std::string middle = filename.substr(prefix.size(), filename.size() - prefix.size() - suffix.size());
            if (middle.empty()) continue;
            const bool isAllDigits = std::all_of(middle.begin(), middle.end(), [](unsigned char c) { return std::isdigit(c); });
            if (!isAllDigits) continue;
            toDelete.emplace_back(std::move(filename));
        }
        if (toDelete.empty()) return;

        size_t deletedCount = 0;
        size_t missingCount = 0;
        size_t failedCount = 0;
        for (const auto& filename : toDelete) {
            if (storage->FileDelete(filename.c_str())) {
                deletedCount++;
                continue;
            }
            // ISteamRemoteStorage::GetFileCount() and file listing can change while we are deleting.
            // If the file is already gone, treat it as a tolerated race and keep it out of "remaining".
            if (!storage->FileExists(filename.c_str())) {
                missingCount++;
                continue;
            }
            failedCount++;
            putlog("Failed to delete old UGC file: %s", filename.c_str());
        }
        putlog("Cleanup old UGC files: candidates=%zu, deleted=%zu, already_gone=%zu, remaining=%zu (%s).",
               toDelete.size(), deletedCount, missingCount, failedCount, boardName.c_str());
    }
};
