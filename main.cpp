#include "./AudioPlayer.cpp"
#include "./PlaylistParser.cpp"
#include "./TerminalGUI.cpp"
#include "./GLGUI.cpp"

PlaylistParser *lists;
int main(int argc, char *argv[]) {
    lists = new PlaylistParser();
    AudioPlayer player = AudioPlayer(lists);
    player.playlist = lists->getRoot();
    if (argc > 1) {
        std::string option = argv[1];
        if (option == "help") {
            std::cout << "Formatting key:\n" << 
                "() - a peice of raw text\n" << 
                "[] - an optional argument\n" << 
                "text - arbitrary name for an argument\n" << 
                "\n" << 
                "List of commands:\n" << 
                "(help) - Report this list\n" << 
                "(versions) - Report various important versions, including the app version\n" << 
                "(categorize), folder[, playlist] - Load the contents of a folder into the local .playlists file, optionally also loading those songs into a particular playlist\n" << 
                "(artist), name[, guiType] - Play all songs by a single artist, guiType is one of none, terminal, or window and defines where the player will be rendered, defaults to terminal\n" << 
                "(album), name[, guiType] - Play all songs in a single album, guiType is one of none, terminal, or window and defines where the player will be rendered, defaults to terminal\n" << 
                "(playlist), name[, guiType] - play all songs inside a playlist, guiType is one of none, terminal, or window and defines where the player will be rendered, defaults to terminal\n" << 
                "(play) path[, guiType] - play a single audio file, guiType is one of none, terminal, or window and defines where the player will be rendered\n" <<
                "(shuffle) playlist - shuffles a playlist then stores the result as the local playlist order\n" <<
                "(sort) playlist - sorts a playlist a-z by title, and then stores that result to disk\n" <<
                "(add) path[, playlist] - adds a single file to the local .playlists, optionally also adds it to a given playlist\n" <<
                "\n" <<
                "List of key binds:\n" <<
                "space - pauses/plays music\n" <<
                "'l' - enables looping\n" <<
                "'s' - shuffles the playlist, does not save\n" <<
                "shift + 's' - sorts the playlist, does not save\n" <<
                "'t' - opens a prompt to set for how long music should play\n" << 
                "arrow up - starts the previous song, will always roll to the end if at the top most song\n" <<
                "arrow down - starts the next song, will only roll to the previous if looping is enabled\n" <<
                "arrow left - rewinds the song by ~1 second\n" <<
                "arrow right - fast forwards the song by ~1 second\n";
            return 0;
        }
        // load in an entire folder of songs as a playlist
        if (option == "categorize") {
            Playlist *list = lists->getRoot();
            if (argc >= 4) list = lists->getPlaylist(argv[3]);
            for (const auto &entry : std::filesystem::directory_iterator(argv[2]))
                lists->addFromPath((char *)entry.path().c_str(), list->id);
            lists->saveToDisk();
            return 0;
        }
        if (option == "add") {
            Playlist *list = lists->getRoot();
            if (argc >= 4) list = lists->getPlaylist(argv[3]);
            lists->addFromPath(argv[2], list->id);
            lists->saveToDisk();
            return 0;
        }
        if (option == "versions") {
            std::cout << "AudioPlayer-1.12.20\n" << sf_version_string() << "\n";
            return 0;
        }
        if (option == "sort") {
            Playlist *list = lists->getRoot();
            if (argc >= 3) list = lists->getPlaylist(argv[2]);
            lists->sortPlaylist(list->id);
            lists->saveToDisk();
            return 0;
        }
        if (option == "shuffle") {
            Playlist *list = lists->getRoot();
            if (argc >= 3) list = lists->getPlaylist(argv[2]);
            lists->shufflePlaylist(list->id);
            lists->saveToDisk();
            return 0;
        }
        if (argc > 3) {
            std::string guiType = argv[1];
            if (guiType == "none") {

            } else if (guiType == "window") {
                player.gui = new GLGUI(&player, lists);
            } else {
                player.gui = new TerminalGUI(&player, lists);
            }
        } else {
            player.gui = new TerminalGUI(&player, lists);
        }
        if (option == "artist") {
            player.playPlaylist(lists->getArtist(argv[2]));
        } else if (option == "album") {
            player.playPlaylist(lists->getAlbum(argv[2]));
        } else if (option == "playlist") {
            player.playPlaylist(lists->getPlaylist(argv[2]));
        } else if (option == "play") {
            player.gui->single = true;
            player.playFile(argv[2], true);
            return 0;
        }
    }
}