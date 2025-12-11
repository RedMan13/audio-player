#ifndef INTERFACE_LOADED
#define INTERFACE_LOADED
#include "./PlaylistParser.cpp"
#include "./AudioPlayer.hh"

class AudioPlayer;
class InterfaceGUI {
    private:
    public:
        bool loop = false; 
        bool shuffle = false;
        bool pause = false;
        int seekTo = 0;
        int nextSong = 0;
        bool exitApp = false;
        bool single = false;
        AudioPlayer *player;
        PlaylistParser *lists;
};
#endif