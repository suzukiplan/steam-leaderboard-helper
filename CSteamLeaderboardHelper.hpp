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
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "steam_api.h"

class CSteamLeaderboardHelper
{
  private:
    CCallResult<CSteamLeaderboardHelper, LeaderboardFindResult_t> callResultFindLeaderboard;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoresDownloaded_t> callResultDownloadLeaderboardScoreTop;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoresDownloaded_t> callResultDownloadLeaderboardScoreMine;
    CCallResult<CSteamLeaderboardHelper, LeaderboardScoreUploaded_t> callResultUploadLeaderboardScore;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageFileWriteAsyncComplete_t> callResultWriteReplay;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageFileShareResult_t> callResultShareReplay;
    CCallResult<CSteamLeaderboardHelper, LeaderboardUGCSet_t> callResultAttachReplay;
    CCallResult<CSteamLeaderboardHelper, RemoteStorageDownloadUGCResult_t> callResultDownloadUGC;
    std::vector<uint8_t> ugcData;
    std::string boardName;
    std::string ugcName;
    bool initialized;
    int maxEntries;
    SteamLeaderboard_t leaderboard;
    std::function<void(const char*)> logger;
    std::function<void(const uint8_t* data, size_t size)> ugcDownloadCallback;

    std::vector<LeaderboardEntry_t> top;
    bool topRanksDownloaded;
    LeaderboardEntry_t myRank;
    bool myRankDownloaded;

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
        : CSteamLeaderboardHelper(std::move(boardName), "replay.dat", std::move(logger))
    {
    }

    /**
     * @brief Constructor
     * @param boardName Leaderboard name
     * @param ugcName UGC filename on Steam Cloud
     * @param logger Log callback
     */
    CSteamLeaderboardHelper(std::string boardName, std::string ugcName, std::function<void(const char*)> logger)
        : leaderboard(0),
          boardName(boardName),
          ugcName(ugcName),
          initialized(false),
          topRanksDownloaded(false),
          myRankDownloaded(false)
    {
        this->logger = std::move(logger);
        ugcData.clear();
        setMaxEntries(100);
    }

    void setMaxEntries(int maxEntries)
    {
        this->maxEntries = maxEntries < 1 ? 1 : maxEntries;
    }

    /**
     * @brief Initializes the helper
     * @remark Automatically reloads entries after initialization.
     */
    void initialize()
    {
        if (initialized) return;
        initialized = true;
        putlog("Initializing Steam leaderboard: %s", boardName.c_str());
        auto hdl = SteamUserStats()->FindLeaderboard(boardName.c_str());
        this->callResultFindLeaderboard.Set(hdl, this, &CSteamLeaderboardHelper::onFindLeaderboard);
    }

    /**
     * @brief Reloads entries (top ranks and current user)
     * @return true: success, false: failed
     */
    bool reload()
    {
        if (!initialized || 0 == leaderboard) {
            putlog("Reload failed: leaderboard is not initialized (%s).", boardName.c_str());
            return false;
        }
        putlog("Reloading leaderboard: %s", boardName.c_str());
        // top ranks
        topRanksDownloaded = false;
        auto handleRanking = SteamUserStats()->DownloadLeaderboardEntries(this->leaderboard, k_ELeaderboardDataRequestGlobal, 1, maxEntries);
        this->callResultDownloadLeaderboardScoreTop.Set(handleRanking, this, &CSteamLeaderboardHelper::onDownloadLeaderboardScoreTop);
        // current user
        myRankDownloaded = false;
        auto handleMine = SteamUserStats()->DownloadLeaderboardEntries(this->leaderboard, k_ELeaderboardDataRequestGlobalAroundUser, 0, 0);
        this->callResultDownloadLeaderboardScoreMine.Set(handleMine, this, &CSteamLeaderboardHelper::onDownloadLeaderboardScoreMine);
        return true;
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
     * @brief Downloads UGC data attached to an entry
     * @param entry Target entry
     * @param callback Called with (data, size) when finished. On failure, (nullptr, 0).
     */
    void downloadUGC(LeaderboardEntry_t* entry, std::function<void(const uint8_t* data, size_t size)> callback)
    {
        ugcDownloadCallback = std::move(callback);
        if (!entry || 0 == entry->m_hUGC) {
            putlog("UGC download failed: invalid entry.");
            if (ugcDownloadCallback) ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            return;
        }
        putlog("Downloading UGC for rank #%d on leaderboard %s.", entry->m_nGlobalRank, boardName.c_str());
        auto hdl = SteamRemoteStorage()->UGCDownload(entry->m_hUGC, 0);
        this->callResultDownloadUGC.Set(hdl, this, &CSteamLeaderboardHelper::onDownloadUGC);
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
        if (!initialized || 0 == leaderboard) {
            putlog("Upload failed: leaderboard is not initialized (%s).", boardName.c_str());
            return false;
        }
        if (data && 0 < size) {
            ugcData.assign(data, data + size);
        } else {
            ugcData.clear();
        }
        auto hdl = SteamUserStats()->UploadLeaderboardScore(this->leaderboard, k_ELeaderboardUploadScoreMethodKeepBest, score, nullptr, 0);
        this->callResultUploadLeaderboardScore.Set(hdl, this, &CSteamLeaderboardHelper::onUploadScore);
        return true;
    }

  private:
    void onFindLeaderboard(LeaderboardFindResult_t* callback, bool failed)
    {
        if (!callback->m_bLeaderboardFound || failed) {
            putlog("Leaderboard not found or request failed: %s", boardName.c_str());
            initialized = false;
            return;
        }
        putlog("Leaderboard found: %s", boardName.c_str());
        this->leaderboard = callback->m_hSteamLeaderboard;
        this->reload();
    }

    void onDownloadLeaderboardScoreTop(LeaderboardScoresDownloaded_t* callback, bool failed)
    {
        if (failed || !callback) {
            putlog("Failed to download leaderboard entries: %s", boardName.c_str());
            return;
        }
        putlog("Downloaded %d entries from leaderboard %s.", callback->m_cEntryCount, boardName.c_str());
        top.clear();
        for (int i = 0; i < callback->m_cEntryCount; i++) {
            LeaderboardEntry_t entry;
            SteamUserStats()->GetDownloadedLeaderboardEntry(callback->m_hSteamLeaderboardEntries, i, &entry, nullptr, 0);
            top.push_back(entry);
        }
        topRanksDownloaded = true;
    }

    void onDownloadLeaderboardScoreMine(LeaderboardScoresDownloaded_t* callback, bool failed)
    {
        if (failed || !callback) {
            putlog("Failed to download current user's leaderboard entry: %s", boardName.c_str());
            return;
        }
        if (0 == callback->m_cEntryCount) {
            putlog("No entry for the current user on leaderboard %s.", boardName.c_str());
            memset(&myRank, 0, sizeof(myRank));
        } else {
            SteamUserStats()->GetDownloadedLeaderboardEntry(callback->m_hSteamLeaderboardEntries, 0, &myRank, nullptr, 0);
        }
        myRankDownloaded = true;
    }

    void onDownloadUGC(RemoteStorageDownloadUGCResult_t* callback, bool failed)
    {
        if (!ugcDownloadCallback) return;
        if (failed || !callback || callback->m_eResult != k_EResultOK || callback->m_nSizeInBytes <= 0) {
            putlog("UGC download failed for leaderboard %s (result=%d).", boardName.c_str(), callback ? callback->m_eResult : -1);
            ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            return;
        }
        ugcData.resize(static_cast<size_t>(callback->m_nSizeInBytes));
        const int32 bytesRead = SteamRemoteStorage()->UGCRead(
            callback->m_hFile,
            ugcData.data(),
            callback->m_nSizeInBytes,
            0,
            k_EUGCRead_ContinueReadingUntilFinished);
        if (bytesRead <= 0) {
            putlog("UGC read failed on leaderboard %s.", boardName.c_str());
            ugcDownloadCallback(nullptr, 0);
            ugcDownloadCallback = nullptr;
            ugcData.clear();
            return;
        }
        ugcData.resize(static_cast<size_t>(bytesRead));
        ugcDownloadCallback(ugcData.data(), ugcData.size());
        ugcDownloadCallback = nullptr;
    }

    void onUploadScore(LeaderboardScoreUploaded_t* callback, bool failed)
    {
        if (failed || !callback || !callback->m_bSuccess) {
            putlog("Failed to upload score to leaderboard %s.", boardName.c_str());
            return;
        }
        if (!callback->m_bScoreChanged) {
            putlog("High score unchanged for leaderboard %s.", boardName.c_str());
            return;
        }
        if (ugcData.empty()) {
            putlog("Score uploaded to leaderboard %s (no UGC attached).", boardName.c_str());
            return;
        }
        putlog("Writing UGC to Steam Cloud.");
        auto hdl = SteamRemoteStorage()->FileWriteAsync(ugcName.c_str(), ugcData.data(), ugcData.size());
        this->callResultWriteReplay.Set(hdl, this, &CSteamLeaderboardHelper::onWriteReplay);
    }

    void onWriteReplay(RemoteStorageFileWriteAsyncComplete_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to write UGC to Steam Cloud (result=%d).", callback ? callback->m_eResult : -1);
            return;
        }
        putlog("Sharing UGC in Steam Cloud.");
        auto hdl = SteamRemoteStorage()->FileShare(ugcName.c_str());
        this->callResultShareReplay.Set(hdl, this, &CSteamLeaderboardHelper::onShareReplay);
    }

    void onShareReplay(RemoteStorageFileShareResult_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to share UGC in Steam Cloud (result=%d).", callback ? callback->m_eResult : -1);
            return;
        }
        putlog("Attaching UGC to leaderboard %s.", boardName.c_str());
        auto hdl = SteamUserStats()->AttachLeaderboardUGC(leaderboard, callback->m_hFile);
        this->callResultAttachReplay.Set(hdl, this, &CSteamLeaderboardHelper::onAttachReplay);
    }

    void onAttachReplay(LeaderboardUGCSet_t* callback, bool failed)
    {
        if (failed || !callback || callback->m_eResult != k_EResultOK) {
            putlog("Failed to attach UGC to the leaderboard (result=%d).", callback ? callback->m_eResult : -1);
            return;
        }
        putlog("Successfully attached UGC to leaderboard %s.", boardName.c_str());
        ugcData.clear();
        this->reload();
    }
};
