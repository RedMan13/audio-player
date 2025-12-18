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
#include <semaphore>
#include <functional>
#include "./PlaylistParser.cpp"
#ifndef PLAYER_LOADED
#define PLAYER_LOADED
#include "./InterfaceGUI.cpp"

AudioPlayer::AudioPlayer(PlaylistParser *lists) {
    ao_initialize();
    driver = ao_default_driver_id();
    this->lists = lists;
}
AudioPlayer::~AudioPlayer() { ao_shutdown(); }

void AudioPlayer::playFile(std::string fileName, bool setMeta) {
    SF_INFO fileFormat;
    SNDFILE *file = sf_open(fileName.c_str(), SFM_READ, &fileFormat);
    if (sf_error(file) > 0) {
        char log[2048];
        sf_command(file, SFC_GET_LOG_INFO, &log, 2048);
        std::cout << log;
        std::cerr << sf_strerror(file) << "\n";
        return;
    }
    sf_command(file, SFC_SET_CLIPPING, NULL, SF_TRUE);

    // make format metadata for ao
    ao_sample_format format;
    format.bits = 16;
    format.rate = fileFormat.samplerate;
    format.channels = fileFormat.channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = "M";
    int *channelMap = new int[fileFormat.channels];
    if (sf_command(file, SFC_GET_CHANNEL_MAP_INFO, channelMap, fileFormat.channels * sizeof(int))) {
        char *matrix = new char[(fileFormat.channels * 3) +1];
        int j = 0;
        for (int i = 0; i < fileFormat.channels; i++) {
            if (i != 0) matrix[j++] = ',';
            switch (channelMap[i]) {
            case SF_CHANNEL_MAP_INVALID: matrix[j++] = 'X'; break;
            case SF_CHANNEL_MAP_MONO: matrix[j++] = 'M'; break;
            case SF_CHANNEL_MAP_LEFT: matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_RIGHT: matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_CENTER: matrix[j++] = 'C'; break;
            case SF_CHANNEL_MAP_REAR_CENTER: matrix[j++] = 'B'; matrix[j++] = 'C'; break;
            case SF_CHANNEL_MAP_REAR_LEFT: matrix[j++] = 'B'; matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_REAR_RIGHT: matrix[j++] = 'B'; matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_LFE: matrix[j++] = 'L'; matrix[j++] = 'F'; matrix[j++] = 'E'; break;
            case SF_CHANNEL_MAP_SIDE_LEFT: matrix[j++] = 'S'; matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_SIDE_RIGHT: matrix[j++] = 'S'; matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_AMBISONIC_B_W: matrix[j++] = 'A'; matrix[j++] = '1'; break;
            case SF_CHANNEL_MAP_AMBISONIC_B_X: matrix[j++] = 'A'; matrix[j++] = '2'; break;
            case SF_CHANNEL_MAP_AMBISONIC_B_Y: matrix[j++] = 'A'; matrix[j++] = '3'; break;
            case SF_CHANNEL_MAP_AMBISONIC_B_Z: matrix[j++] = 'A'; matrix[j++] = '4'; break;
            // bad conversions; libao doesnt support these
            case SF_CHANNEL_MAP_FRONT_LEFT: matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_FRONT_RIGHT: matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_FRONT_CENTER: matrix[j++] = 'C'; break;
            case SF_CHANNEL_MAP_FRONT_LEFT_OF_CENTER: matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_FRONT_RIGHT_OF_CENTER: matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_TOP_CENTER: matrix[j++] = 'C'; break;
            case SF_CHANNEL_MAP_TOP_FRONT_LEFT: matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_TOP_FRONT_RIGHT: matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_TOP_FRONT_CENTER: matrix[j++] = 'C'; break;
            case SF_CHANNEL_MAP_TOP_REAR_LEFT: matrix[j++] = 'B'; matrix[j++] = 'L'; break;
            case SF_CHANNEL_MAP_TOP_REAR_RIGHT: matrix[j++] = 'B'; matrix[j++] = 'R'; break;
            case SF_CHANNEL_MAP_TOP_REAR_CENTER: matrix[j++] = 'B'; matrix[j++] = 'C'; break;
            // case SF_CHANNEL_MAP_MAX: break; // dont know what this does, no suitable conversion
            }
        }
        matrix[j] = '\0';
        format.matrix = matrix;
    } else {
        // ooooonooooo we cant read what the map is.
        // guess by channel count, otherwise just assume all channels go to the one mono output
        switch (fileFormat.channels) {
        case 2: format.matrix = "L,R"; break;
        case 4: format.matrix = "L,R,BL,BR"; break;
        case 6: format.matrix = "L,R,C,LFE,BR,BL"; break;
        case 8: format.matrix = "L,R,C,LFE,BR,BL,SL,SR"; break;
        default:
            char *matrix = new char[(fileFormat.channels * 2) +1];
            int j = 0;
            for (int i = 0; i < fileFormat.channels; i++) {
                if (i != 0) matrix[j++] = ',';
                matrix[j++] = 'M';
            }
            matrix[j] = '\0';
            format.matrix = matrix;
            break;
        }
    }
    ao_device *device = ao_open_live(driver, &format, NULL);
    if (device == NULL) {
        std::cerr << "Error opening device. " << errno << "; ";
        switch (errno) {
        case AO_ENODRIVER: std::cerr << "No device could be found under " << driver; break;
        case AO_ENOTLIVE: std::cerr << "The driver " << driver << " could not be used for live audio"; break;
        case AO_EBADOPTION: std::cerr << "An option provided for the driver was invalid"; break;
        case AO_EOPENDEVICE: std::cerr << "The live audio device could not be opened"; break;
        default:
        case AO_EFAIL: std::cerr << "Unhandled error"; break;
        }
        std::cerr << "\n";
        return;
    }

    if (setMeta) {
        const char *artist = sf_get_string(file, SF_STR_ARTIST);
        const char *album = sf_get_string(file, SF_STR_ALBUM);
        const char *title = sf_get_string(file, SF_STR_TITLE);
        if (artist != NULL) this->artist = artist;
        if (album != NULL) this->album = album;
        if (title != NULL) this->title = title;
    }
    frameRate = fileFormat.samplerate;
    numFrames = fileFormat.frames;
    channels = fileFormat.channels;
    frame = 0;
    // manually pipe data between sndfile and ao
    int frameCount = (fileFormat.samplerate / 4) > MAX_BUFFER ? MAX_BUFFER : (fileFormat.samplerate / 4);
    arrayLength = frameCount * fileFormat.channels;
    int iter = ceil((float)(fileFormat.frames) / (float)(frameCount));
    buffer = new short[arrayLength];

    int bytesPerSample = ceil(format.bits / 8);
    for (int i = 0; i < iter; i++) {
        if (gui->pause) { i--; continue; }
        if (gui->exitApp) break;
        if (gui->nextSong != 0) break;
        if (gui->seekTo != 0) {
            frame += gui->seekTo;
            if (frame >= numFrames) frame = numFrames -1;
            if (frame < 0) frame = 0;
            sf_seek(file, frame, SF_SEEK_SET);
            gui->seekTo = 0;
        }
        arrayLength = sf_read_short(file, buffer, arrayLength);
        ao_play(device, (char *)buffer, arrayLength * bytesPerSample);
        frame += arrayLength / fileFormat.channels;
    }

    // done playing: close the file and the audio interface
    sf_close(file);
    ao_close(device);
}
void AudioPlayer::playPlaylist(Playlist *playlist) {
    this->playlist = playlist;
    if (playlist->songs.size() <= 0) return;
    if (gui->shuffle) lists->shufflePlaylist(playlist->id);
    do {
        if (!gui->loop && playing >= playlist->songs.size()) break;
        playFile(playlist->songs[playing]->path, false);
        if (gui->exitApp) break;
        if (gui->nextSong != 0) playing += gui->nextSong;
        else playing++;
        if (playing < 0) playing = playlist->songs.size() -1;
        gui->nextSong = 0;
        if (gui->loop && playing >= playlist->songs.size()) {
            playing = 0;
            if (gui->shuffle) lists->shufflePlaylist(playlist->id);
        }
    } while (true);
}
#endif