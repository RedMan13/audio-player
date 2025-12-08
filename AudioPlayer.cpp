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
#include "./PlaylistParser.cpp"

// max buffer size, normally the buffer will ensure the file is devided into one second 
// chunks, except if the requested size excedes this limit
#define MAX_BUFFER 16777216
int driver;
// not actual frames!!!!
// these are the frames as defined by sndfile, as in samples
int numFrames;
int frame;
int frameRate;
int playing = 0;
int scrollAdvance = 0;
PlaylistParser *lists;
Playlist *playlist;
bool loop = false; 
bool shuffle = false;
bool single = false;
bool pause = false;
auto start = std::chrono::high_resolution_clock::now();
int timeToClose = -1;
bool runInputs = true;
int seekTo = 0;
int nextSong = 0;
bool exitApp = false;

winsize getTerminalSize() {
    winsize out;
    // dimensions of stdout
    ioctl(1, TIOCGWINSZ, &out);

    return out;
}

int strLen(std::string str) {
    int len = 0;
    for (int i = 0; i < str.length(); i++) {
        len++;
        if (!(str[i] & 0x80)) continue;
        if ((str[i] & 0xE0) == 0xC0) i++;
        if ((str[i] & 0xF0) == 0xE0) i++;
        if ((str[i] & 0xF8) == 0xF0) i++;
    }
    return len;
}

std::string secondsToString(int time) {
    std::string seconds = std::to_string(time % 60);
    std::string minutes = std::to_string((time / 60) % 60);
    std::string hours = std::to_string((time / 60) / 60);
    seconds = std::string(2 - seconds.length(), '0') + seconds;
    minutes = std::string(2 - minutes.length(), '0') + minutes;
    if (hours == "0") return minutes + ":" + seconds;
    return hours + ":" + minutes + ":" + seconds;
}
std::string framesToString(int frames) {
    return secondsToString((int)round((float)frames / (float)frameRate));
}
void drawGUI() {
    winsize size = getTerminalSize();
  
    float prog = ((float)frame / (float)numFrames) * size.ws_col;
    int y = 0;
    scrollAdvance++;
    std::cout << "\x1b[0m\x1b[2J";
    int songs = playlist->songs.size();
    int height = size.ws_row;
    for (int i = playing - (height / 2); i < songs && y < height; i++) {
        y++;
        if (i < 0) continue;
        std::string left = framesToString(frame);
        std::string right = framesToString(numFrames);
        if (i != playing) { left = ""; right = ""; }
        left += std::string(10.0 - left.length() <= 0 ? left.length() +1 : 10 - left.length(), ' ');
        right = std::string(10.0 - right.length() <= 0 ? right.length() +1 : 10 - right.length(), ' ') + right;
        std::string line;
        int wouldBeWidth = left.length() + right.length() + 5 + playlist->songs[i]->title.length() + playlist->songs[i]->artist.length();
        std::cout << "\x1b[" << y << ";1H";
        if (i == playing) std::cout << "\x1b[97m";
        else std::cout << "\x1b[90m";
        if (wouldBeWidth > size.ws_col && i == playing) {
            line = left;

            // draw the scrolling text banner
            std::string scroller = ";  By: " + playlist->songs[i]->artist + "            " + playlist->songs[i]->title;
            int scrollArea = size.ws_col - (left.length() + right.length());
            int scrollLen = strLen(scroller);
            int charIdx = (scrollAdvance % scrollLen);
            bool opened = false;
            for (int j = 0; j < scrollArea; j++) {
                // caught int he middle of a multi-parter, skip out until we are not
                if ((scroller[charIdx] & 0xC0) == 0x80 && !opened) {
                    charIdx++;
                    j--;
                    continue;
                }
                // still special bit high, and its not int he middle, so its an opener
                if (scroller[charIdx] & 0x80) opened = true;
                line += scroller[charIdx];
                charIdx = (charIdx +1) % scrollLen;
            }
            line += right;
        } else {
            bool rightShrunk = false;
            left += playlist->songs[i]->title;
            int titleLen = strLen(playlist->songs[i]->title);
            int artistLen = strLen(playlist->songs[i]->artist);
            int leftLen = strLen(left);
            if (((artistLen + 6 + right.length()) + leftLen) >= size.ws_col) {
                if (leftLen > (size.ws_col / 2))
                    left = left.substr(0, (size.ws_col / 2) - 4) + "... ";
                if ((artistLen + 6 + right.length()) > (size.ws_col / 2)) {
                    right = "By: " + playlist->songs[i]->artist.substr(0, (size.ws_col / 2) - (8 + right.length())) + "... " + right;
                    rightShrunk = true;
                }
            }
            if (!rightShrunk) right = "By: " + playlist->songs[i]->artist + right;
            line = left + std::string(size.ws_col - (strLen(right) + strLen(left)), ' ') + right;
        }
        bool hasFlipped = false;
        if (i == playing) std::cout << "\x1b[4m";
        for (int j = 0; j < line.length(); j++) {
            std::cout << line[j];
            if (j >= prog && !hasFlipped) {
                std::cout << "\x1b[24m";
                hasFlipped = true;
                continue;
            }
        }
        std::cout << "\x1b[24m";
    }

    if (pause) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) << ";1Hpaused\x1b[0m";
    if (loop) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) << ";" << (size.ws_col -3) << "Hloop\x1b[0m";
    if (height > 2) {
        if (timeToClose != -1) {
            auto current = std::chrono::high_resolution_clock::now();
            int since = (((current - start).count() / 1000) / 1000) / 1000;
            if ((timeToClose - since) <= 0) {
                exitApp = true;
            } else {
                std::string timeStamp = secondsToString(timeToClose - since);
                std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) +2 << ";1H" << timeStamp << "\x1b[0m";
            }
        }
        if (shuffle) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) +2 << ";" << (size.ws_col -7) << "Hshuffle\x1b[0m";
    }
    std::cout << "\x1b[" << size.ws_row -1 << ":1H" << "\n";
}

int parseInt(std::string str) {
    int out = 0;
    int mul = 1;
    for (int i = str.length() -1; i >= 0; i--) {
        if (str[i] < 48 || str[i] > 58) continue;
        out += (str[i] - 48) * mul;
        mul *= 10;
    }
    return out;
}
// this gets run under a seperate thread due to weird blocking shenanigans
void inputProc() {
    initscr();
    noecho();
    int stage = 0;
    while (runInputs) {
        timeout(250);
        int key = getch();
        drawGUI();
        if (stage > 2) stage = 0;
        if (key == -1) continue;
        switch (stage) {
        case 0:
            switch (key) {
            case '\x1b':
                stage = 1;
                break;
            case 'l':
                loop = !loop;
                break;
            case ' ':
                pause = !pause;
                break;
            case 's': {
                Song *currentSong = playlist->songs[playing];
                lists->shufflePlaylist(playlist->id);
                shuffle = true;
                for (int i = 0; i < playlist->songs.size(); i++) {
                    if (playlist->songs[i] == currentSong) {
                        playing = i;
                        break;
                    }
                }
                break;
            }
            case 'S': {
                Song *currentSong = playlist->songs[playing];
                lists->sortPlaylist(playlist->id);
                shuffle = false;
                for (int i = 0; i < playlist->songs.size(); i++) {
                    if (playlist->songs[i] == currentSong) {
                        playing = i;
                        break;
                    }
                }
                break;
            }
            case 't':
                std::cout << "\x1b[1;1HEnter a time in the format [hours:]minutes:seconds: \x1b[1;1H\n";
                echo();
                char input = 0;
                timeout(-1);
                getstr(&input);
                noecho();
                std::string timeCode = &input;
                if (timeCode.find_first_of(':') == -1) break;
                if (timeCode.find_last_of(':') == -1) break;
                timeToClose = 0;
                std::string minutes = timeCode.substr(0, timeCode.find_last_of(':'));
                if (timeCode.find_last_of(':') != timeCode.find_first_of(':')) {
                    timeToClose += parseInt(timeCode.substr(0, timeCode.find_first_of(':'))) * 60 * 60;
                    minutes = timeCode.substr(timeCode.find_first_of(':') +1, timeCode.find_last_of(':'));
                }
                timeToClose += parseInt(minutes) * 60;
                timeToClose += parseInt(timeCode.substr(timeCode.find_last_of(':') +1));
                start = std::chrono::high_resolution_clock::now();
                break;
            }
            break;
        case 1:
            if (key != '[') {
                stage = 0;
                break;
            }
            stage = 2;
            break;
        case 2:
            switch (key) {
            case 'A': nextSong--; break;
            case 'B': nextSong++; break;
            case 'C':
                seekTo += frameRate;
                break;
            case 'D':
                seekTo -= frameRate;
                break;
            }
            stage = 0;
            break;
        default: stage = 0; break;
        }
    }
    endwin();
}

short *buffer;
int arrayLength;
bool needsChunk = true;
bool runDecoder = true;
void decoderThread(SNDFILE *file) {
    runDecoder = true;
    while (runDecoder) {
        if (!needsChunk) continue;
        buffer = new short[arrayLength];
        arrayLength = sf_read_short(file, buffer, arrayLength);
        needsChunk = false;
    }
}
void playFile(std::string fileName, bool setMeta) {
    SF_INFO fileFormat;
    SNDFILE *file = sf_open(fileName.c_str(), SFM_READ, &fileFormat);
    if (sf_error(file) > 0) {
        char log[2048];
        sf_command(file, SFC_GET_LOG_INFO, &log, 2048);
        std::cout << log;
        std::cerr << sf_strerror(file) << "\n";
        return;
    }

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

    frameRate = fileFormat.samplerate;
    numFrames = fileFormat.frames;
    frame = 0;
    // manually pipe data between sndfile and ao
    int frameCount = (fileFormat.samplerate / 3) > MAX_BUFFER ? MAX_BUFFER : (fileFormat.samplerate / 3);
    arrayLength = frameCount * fileFormat.channels;
    int iter = ceil((float)(fileFormat.frames) / (float)(frameCount));
    std::thread decoder(decoderThread, file);

    for (int i = 0; i < iter; i++) {
        // wait for a new buffer to appear
        if (buffer == NULL) { i--; continue; }
        if (exitApp) break;
        if (pause) { i--; continue; }
        if (nextSong != 0) break;
        if (seekTo != 0) {
            frame += seekTo;
            if (frame >= numFrames) frame = numFrames -1;
            if (frame < 0) frame = 0;
            sf_seek(file, frame, SF_SEEK_SET);
            seekTo = 0;
        }
        needsChunk = true;
        ao_play(device, (char*)buffer, arrayLength * (format.bits / 8));
        frame += arrayLength / fileFormat.channels;
    }
    runDecoder = false;
    decoder.join();

    // done playing: close the file and the audio interface
    sf_close(file);
    ao_close(device);
}

int main(int argc, char *argv[]) {
    lists = new PlaylistParser();
    playlist = lists->getRoot();
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
                "(artist), name - Play all songs by a single artist\n" << 
                "(album), name - Play all songs in a single album\n" << 
                "(playlist), name - play all songs inside a playlist\n" << 
                "(play) path - play a single audio file\n" <<
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
            std::cout << "AudioPlayer-1.10.4\n" << sf_version_string() << "\n";
            return 0;
        }
        if (option == "play") {
            std::thread gui(inputProc);
            ao_initialize();
            single = true;
            playFile(argv[2], true);
            runInputs = false;
            gui.join();
            ao_shutdown();
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
        if (option == "artist") {
            playlist = lists->getArtist(argv[2]);
        } else if (option == "album") {
            playlist = lists->getAlbum(argv[2]);
        } else if (option == "playlist") {
            playlist = lists->getPlaylist(argv[2]);
        }
    }
    if (playlist->songs.size() <= 0) return 0;
    std::thread gui(inputProc);
    ao_initialize();
    driver = ao_default_driver_id();
    if (shuffle) lists->shufflePlaylist(playlist->id);
    do {
        if (!loop && playing >= playlist->songs.size()) break;
        playFile(playlist->songs[playing]->path, false);
        if (exitApp) break;
        if (nextSong != 0) playing += nextSong;
        else playing++;
        if (playing < 0) playing = playlist->songs.size() -1;
        nextSong = 0;
        if (loop && playing >= playlist->songs.size()) {
            playing = 0;
            if (shuffle) lists->shufflePlaylist(playlist->id);
        }
    } while (true);
    runInputs = false;
    gui.join();
    ao_shutdown();
}
