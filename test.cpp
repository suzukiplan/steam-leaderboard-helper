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

    puts("Start main loop");
    auto entry = board.getEntry(0);
    while (!entry) {
        SteamAPI_RunCallbacks();
        usleep(100000); // wait 100ms (10fps)
    }
    for (int i = 0; i < 100; i++) {
        entry = board.getEntry(0);
        printf("No.%d score=%d, user=%s\n", entry->m_nGlobalRank, entry->m_nScore, board.getUserName(entry));
    }
    return 0;
}
