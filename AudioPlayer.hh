#include <ao/ao.h>
#include <iostream>
#include <string>
#include <sndfile.h>
#include <math.h>
#include <sys/ioctl.h>
#include <vector>
#include <ncurses.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>
#include "./PlaylistParser.cpp"
#ifndef PLAYER_HEADER_LOADED
#define PLAYER_HEADER_LOADED
#include "./InterfaceGUI.cpp"
#include "./AudioPlayer.hh"

// max buffer size, normally the buffer will ensure the file is devided into one second 
// chunks, except if the requested size excedes this limit
#define MAX_BUFFER 16777216
class InterfaceGUI;
class AudioPlayer {
    private:
        int driver;
    public:
        // not actual frames!!!!
        // these are the frames as defined by sndfile, as in samples
        std::string artist;
        std::string album;
        std::string title;
        int numFrames;
        int frame;
        int channels;
        int frameRate;
        short *buffer;
        int arrayLength;
        int playing = 0;
        Playlist *playlist;
        InterfaceGUI *gui;
        PlaylistParser *lists;

        AudioPlayer(PlaylistParser *lists);
        ~AudioPlayer();

        void playFile(std::string fileName, bool setMeta);
        void playPlaylist(Playlist *playlist);
};
#endif