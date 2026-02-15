# [WIP] Steam Leaderboard Helper

A helper for working with Steam Leaderboards easily.

Because this helper supports attaching and downloading UGC data, you can, for example, implement a “leaderboard with replay data” with minimal effort.

## WIP status

This helper is extracted and refactored from the leaderboard implementation used in [Battle AirForce](https://store.steampowered.com/app/3476490/Battle_AirForce/).

It is currently being prepared for the leaderboard implementation in [Battle Hanafuda](https://store.steampowered.com/app/4161340/Battle_Hanafuda/), and full verification has not been completed yet.

Once verification is complete in Battle Hanafuda, the `[WIP]` tag will be removed.

## How to use

1. Add [CSteamLeaderboardHelper.hpp](./CSteamLeaderboardHelper.hpp) to your C++ project.
2. Include it and prepare a `CSteamLeaderboardHelper` instance **for each leaderboard**.
3. After initializing the Steamworks SDK, call `CSteamLeaderboardHelper::initialize` (initialization & entry download).
4. Submit a score with `CSteamLeaderboardHelper::sendScore`.
   - `sendScore` returns `false` while another score submission is in progress (busy).
   - You can also check `CSteamLeaderboardHelper::isSendScoreBusy`.
5. Retrieve an entry with `CSteamLeaderboardHelper::getEntry`.
6. Download UGC data with `CSteamLeaderboardHelper::downloadUGC`.
   - `downloadUGC` calls the callback with `(nullptr, 0)` while another UGC download is in progress (busy).
