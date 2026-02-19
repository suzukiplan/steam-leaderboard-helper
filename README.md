# [WIP] Steam Leaderboard Helper

A lightweight C++ helper class for working with **Steam Leaderboards** via the Steamworks SDK.

This helper wraps the asynchronous Steam API calls required for:

- Finding a leaderboard
- Downloading top entries
- Downloading the current user’s entry
- Uploading scores
- Attaching UGC (e.g. replay data)
- Downloading attached UGC

With this class, you can implement features such as:

- Leaderboards with replay files
- Ghost data downloads
- Score + metadata storage via Steam Cloud

## WIP Status

This helper is extracted and refactored from the leaderboard implementation used in  
[Battle AirForce](https://store.steampowered.com/app/3476490/Battle_AirForce/).

It is currently being prepared for use in  
[Battle Hanafuda](https://store.steampowered.com/app/4161340/Battle_Hanafuda/), and full production verification is still in progress.

Once verification is complete in Battle Hanafuda, the `[WIP]` tag will be removed.

# Requirements

- C++17 or later
- Steamworks SDK (confirmed: v1.63)
- Regular Steam callback pumping (`SteamAPI_RunCallbacks()`)

This helper does **not** manage SteamAPI lifecycle.  
You must initialize and shut down Steam yourself.

# Design Overview

The helper internally manages:

- `CCallResult<>` wrappers for asynchronous API calls
- State machines for:
  - Initialization
  - Entry downloads
  - Score uploads
  - UGC upload flow
- Automatic reload after successful score upload (optional)

Each `CSteamLeaderboardHelper` instance handles **one leaderboard only**.

If your game uses multiple leaderboards, create one instance per leaderboard.

# Basic Usage

## 1. Add the Header

Add:

```
CSteamLeaderboardHelper.hpp
```

to your project.

Include it after Steam is available:

```cpp
#include "steam_api.h"
#include "CSteamLeaderboardHelper.hpp"
```

## 2. Create an Instance

Create one instance per leaderboard:

```cpp
CSteamLeaderboardHelper leaderboard(
    "GLOBAL_SCORE",
    [](const char* msg) {
        printf("[Leaderboard] %s\n", msg);
    }
);
```

You may optionally specify a UGC filename:

```cpp
CSteamLeaderboardHelper leaderboard(
    "GLOBAL_SCORE", // Board name
    "replay",       // UGC filename base name
    loggerFunction  // Logger callback
);
```

You may also specify a UGC size limit (in bytes).  
The default is **1MB** (`1024^2` bytes):

```cpp
CSteamLeaderboardHelper leaderboard(
    "GLOBAL_SCORE",      // Board name
    "replay",            // UGC filename base name
    256 * 1024,          // UGC size limit (bytes)
    loggerFunction       // Logger callback
);
```

**Notes about `ugcBaseName` (UGC filename base name):**

- When you call `sendScore(score, data, size)`, the helper automatically uploads the UGC as `"{boardName}_{ugcBaseName}_{timestamp}.dat"` (timestamp is the numeric value returned by `time()`).
- After a successful attach (`AttachLeaderboardUGC`), the helper enumerates remote storage and deletes older files that match `"{boardName}_{ugcBaseName}_<digits>.dat"` **except** the one it just attached.

> Filename uses a sanitized version of `boardName` (non-alnum chars are replaced with `_`)

## 3. Initialize

After SteamAPI_Init():

```cpp
leaderboard.initialize();
```

This performs:

1. `FindLeaderboard`
2. Automatic `reload()` after success

Initialization is asynchronous.

You must continue calling:

```cpp
SteamAPI_RunCallbacks();
```

in your main loop.

## 4. Wait Until Ready

Wait until done:

```cpp
while (!leaderboard.isDone()) {
    SteamAPI_RunCallbacks();
}
```

Handle error:

```cpp
if (leaderboard.hasError()) {
    // initialization or download failed
}
```

`isReady()` becomes true when:

- Initialization succeeded
- Top entries downloaded successfully
- Current user entry downloaded successfully

The difference of `isDone`:

- `isDone`: finished (success or error)
- `isReady`: successfully initialized and **both downloads succeeded**

# Reading Leaderboard Data

With this helper, you can retrieve the global ranking list (positions 1 to 100) and the ranking data for the currently registered user.

If the current user is ranked 101st or lower, it is assumed that you will display 101 records as follows:

```
> No.1    Player1         10000 pts     <--- Scroll Top
  No.2    Player2          9000 pts
  No.3    Player3          8000 pts
   :
  No.98   Player98          100 pts
  No.99   Player99           90 pts
  No.100  Player100          80 pts
  No.100+ CurrentUser        10 pts     <--- Scroll Bottom
-------------------------------------
         [A] Replay [B] Back
```

By using this approach, you can load everything in a single batch while omitting the implementation of incremental (“load more”) fetching.

> I think there is room for improvement in cases where you want to view the replays of users around your own rank, but.

## Get Top Entries

```cpp
for (int i = 0; i < 10; i++) {
    auto* entry = leaderboard.getEntry(i);
    if (!entry) continue;

    printf("Rank %d Score %d\n",
           entry->m_nGlobalRank,
           entry->m_nScore);
}
```

## Get Current User Entry

```cpp
auto* mine = leaderboard.getMyEntry();
if (mine) {
    printf("My rank: %d\n", mine->m_nGlobalRank);
}
```

## Get Player Name

```cpp
const char* name = leaderboard.getUserName(entry);
if (name) {
    printf("User: %s\n", name);
}
```

If Steam does not yet have persona information cached,
the helper automatically requests it.

# Reloading

You may manually reload entries:

```cpp
if (leaderboard.canReload()) {
    leaderboard.reload();
}
```

Reload downloads:

- Global top entries (1 → `maxEntries`)
- Current user entry

Busy status:

```cpp
if (leaderboard.isReloadBusy()) {
    // still downloading
}
```

# Submitting Scores

## Simple Score Submission

```cpp
leaderboard.sendScore(score);
```

The upload method is:

```
k_ELeaderboardUploadScoreMethodKeepBest
```

**(Remarks)**

- After a successful score upload, the helper attempts to reload entries. If a UGC download is in progress, this reload is deferred and will be executed automatically once the download finishes (and no other reload is in progress).


## Submit Score with UGC (Replay Data)

```cpp
leaderboard.sendScore(score, replayData, replaySize);
```

If `replaySize` exceeds the configured `ugcSizeLimit`, `sendScore()` returns `false` and the score is **not** uploaded.

UGC upload flow:

1. Upload score
2. Write file to Steam Cloud
3. Share file
4. Attach file to leaderboard entry

If any step fails, the process aborts safely.

**(Remarks)**

- UGC is attached only when the leaderboard score is updated (m_bScoreChanged). If the score is unchanged, the upload flow stops and no UGC is uploaded.
- After a successful score upload, the helper attempts to reload entries. If a UGC download is in progress, this reload is deferred and will be executed automatically once the download finishes (and no other reload is in progress).

## Busy Check

```cpp
if (leaderboard.isSendScoreBusy()) {
    // prevent duplicate submissions
}
```

`sendScore()` returns `false` if already busy.

# Downloading UGC

To download replay data from an entry:

```cpp
leaderboard.downloadUGC(entry,
    [](const uint8_t* data, size_t size) {
        if (!data) {
            printf("Download failed\n");
            return;
        }
        // use replay data
    }
);
```

If the attached UGC size exceeds the configured `ugcSizeLimit`, `downloadUGC()` fails without attempting to download.

Busy state:

```cpp
if (leaderboard.isDownloadBusyUGC()) {
    // wait before issuing another request
}
```

If another download is in progress, the callback receives `(nullptr, 0)`.

# Changing Max Entries

Default maximum downloaded entries: **100**

Change it before initialization:

```cpp
leaderboard.setMaxEntries(50);
```

# Threading Model

This helper assumes:

- All Steam callbacks are executed on the same thread
- `SteamAPI_RunCallbacks()` is called regularly
- No concurrent access from multiple threads

It is **NOT Thread-Safe**.

# Error Handling Philosophy

- Errors are logged through the provided logger callback.
- Failure in one step does not crash the helper.
- Score submission safely resets state.
- Reload only runs when previous reload completed.

# Known Limitations

- Single leaderboard per instance
- Not thread-safe
- No automatic retry mechanism
- UGC filename is shared per instance
- Steamworks SDK v1.63: `ISteamRemoteStorage::GetUGCDetails()`’s third parameter is **non-const** and must not be passed as nullptr; pass a pointer variable instead (passing nullptr may crash). This helper already handles this internally.
- WIP: Production validation ongoing

# Recommended Use Case

This helper is especially useful when implementing:

- Replay-based scoreboards
- Ghost systems
- Competitive score tracking
- Steam Cloud–backed metadata storage

# License

[MIT](./LICENSE.txt)
