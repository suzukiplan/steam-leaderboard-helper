#include <stdio.h>
#include <unistd.h>
#include "CSteamLeaderboardHelper.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        puts("usage: test board_name");
        return 1;
    }
    const char* boardName = argv[1];
    if (!SteamAPI_Init()) {
        puts("SteamAPI_Init failed.");
        return -1;
    }

    CSteamLeaderboardHelper board(boardName, [](const char* msg) {
        puts(msg);
    });
    board.initialize();

    // waiting for download leaderboard
    puts("Start main loop");
    auto entry = board.getEntry(0);
    while (!entry) {
        SteamAPI_RunCallbacks();
        usleep(100000); // wait 100ms (10fps)
        entry = board.getEntry(0);
    }

    // display ranking
    for (int i = 0; i < 100; i++) {
        entry = board.getEntry(i);
        if (!entry) {
            break;
        }
        printf("No.%d score=%d, user=%s\n", entry->m_nGlobalRank, entry->m_nScore, board.getUserName(entry));
    }

    // download UGC data (top)
    bool downloaded = false;
    board.downloadUGC(board.getEntry(0), [&](const uint8_t* data, size_t size) {
        printf("Downloaded %zu bytes.\n", size);
        downloaded = true;
    });
    while (board.isDownloadBusyUGC()) {
        SteamAPI_RunCallbacks();
        usleep(100000); // wait 100ms (10fps)
    }
    if (!downloaded) {
        puts("Can not donwloaded UGC data.");
    }

    return 0;
}
