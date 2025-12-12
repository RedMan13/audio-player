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
        short *buffer;
        short *firstBuffer;
        short *secondBuffer;
        bool onFirstBuffer = true;
        int arrayLength;
        std::mutex needsChunk;
        bool runDecoder = true;
        void decoderThread(SNDFILE *file);
    public:
        // not actual frames!!!!
        // these are the frames as defined by sndfile, as in samples
        std::string artist;
        std::string album;
        std::string title;
        int numFrames;
        int frame;
        int frameRate;
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