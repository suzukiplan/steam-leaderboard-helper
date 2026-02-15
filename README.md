# Steam Leaderboard Helper

A helper for working with Steam Leaderboards easily.

Because this helper supports attaching and downloading UGC data, you can, for example, implement a “leaderboard with replay data” with minimal effort.

## How to use

1. Add [CSteamLeaderboardHelper.hpp](./CSteamLeaderboardHelper.hpp) to your C++ project.
2. Include it and prepare a `CSteamLeaderboardHelper` instance **for each leaderboard**.
3. After initializing the Steamworks SDK, call `CSteamLeaderboardHelper::initialize` (initialization & entry download).
4. Submit a score with `CSteamLeaderboardHelper::sendScore`.
5. Retrieve an entry with `CSteamLeaderboardHelper::getEntry`.
6. Download UGC data with `CSteamLeaderboardHelper::downloadUGC`.
