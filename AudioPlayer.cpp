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
std::string title;
std::string artist;
Playlist *playlist;
bool loop = false;
bool shuffle = false;
bool single = false;

winsize getTerminalSize() {
    winsize out;
    // dimensions of stdout
    ioctl(1, TIOCGWINSZ, &out);

    return out;
}

std::string framesToString(int frames) {
    std::string seconds = std::to_string((int)round((float)frames / (float)frameRate) % 60);
    std::string minutes = std::to_string((int)round(((float)frames / (float)frameRate) / 60.0) % 60);
    std::string hours = std::to_string((int)round((((float)frames / (float)frameRate) / 60.0) / 60.0));
    seconds = std::string(2 - seconds.length(), '0') + seconds;
    minutes = std::string(2 - minutes.length(), '0') + minutes;
    if (hours == "0") return minutes + ":" + seconds;
    return hours + ":" + minutes + ":" + seconds;
}
void drawGUI() {
    winsize size = getTerminalSize();
  
    float prog = ((float)frame / (float)numFrames) * size.ws_col;
    std::string left = framesToString(frame) + " " + title;
    std::string right = "By: " + artist + " " + framesToString(numFrames);
    std::cout << "\x1b[0m\x1b[2J";
    if ((left.length() + right.length()) > size.ws_col) {
        left = framesToString(frame) + " ";
        right = " " + framesToString(numFrames);

        // draw the scrolling text banner
        std::string scroller = title + ";  By: " + artist + "            ";
        std::cout << "\x1b[1;" << left.length() << "H";
        scrollAdvance++;
        int scrollArea = size.ws_col - (left.length() + right.length());
        int charIdx = (scrollAdvance % scroller.length());
        for (int j = 0; j < scroller.length(); j++) {
            std::cout << scroller[charIdx];
            charIdx = (charIdx +1) % scroller.length();
        }
    }
    std::cout << "\x1b[1;1H";
    std::cout << left;
    std::cout << "\x1b[1;" << (size.ws_col - right.length()) +1 << "H";
    std::cout << right << "\n\x1b[97m\x1b[2;1H";
    bool hasFlipped = false;
    for (int i = 0; i < size.ws_col; i++) {
        if (hasFlipped) {
            std::cout << "─";
            continue;
        }
        if (i >= prog) {
            hasFlipped = true;
            if ((prog - i) < -0.25 && (prog - i) > -0.75)
                std::cout << "╾";
            else if ((prog - i) < -0.75)
                std::cout << "─";
            else if ((prog - i) > -0.25)
                std::cout << "━";
            std::cout << "\x1b[90m";
            continue;
        }
        std::cout << "━";
    }
    std::cout << "\x1b[3;1H\x1b[0m";
    std::cout << std::string(((float)size.ws_col - 7.0) / 2, ' ');
    std::cout << "Up Next\n";
    if (single) return;
    int y = 4;
    bool firstLine = true;
    for (int i = playing +1; i < playlist->songs.size() && y < size.ws_row; i++) {
        std::string line = "  " + playlist->songs[i]->title;
        line += std::string(size.ws_col - (line.length() % size.ws_col), ' ');
        if (firstLine) std::cout << "\x1b[97m";
        std::cout << "\x1b[" << y << ";1H";
        int height = ceil((float)line.length() / (float)size.ws_col);
        for (int i = 0, x = 0, ya = 1; i < line.length(); i++) {
            if (ya == height && x == 0 && firstLine)
                std::cout << "\x1b[4m";
            std::cout << line[i];
            x++;
            if (x >= size.ws_col) {
                x = 0;
                ya++;
                y++;
            }
        }
        std::cout << "\n";
        if (firstLine) std::cout << "\x1b[90m\x1b[24m";
        firstLine = false;
    }
}

bool runInputs = true;
int seekTo = 0;
int nextSong = 0;
// this gets run under a seperate thread due to weird blocking shenanigans
void inputProc() {
    initscr();
    int stage = 0;
    while (runInputs) {
        int key = getch();
        if (key == -1) continue;
        std::cout << "\x1b[2K";
        switch (stage) {
        case 0:
            switch (key) {
            case '\x1b':
                stage = 1;
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
            break;
        }
    }
    endwin();
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
    format.matrix = (char*)"M";
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
    const char *artistChars = sf_get_string(file, SF_STR_ARTIST);
    const char *titleChars = sf_get_string(file, SF_STR_TITLE);
    if (setMeta) {
        if (artistChars == NULL) artist = "Unknown";
        else artist = artistChars;
        if (titleChars == NULL) title = fileName;
        else title = titleChars;
    }
    // manually pipe data between sndfile and ao
    int frameCount = (fileFormat.samplerate / 4) > MAX_BUFFER ? MAX_BUFFER : (fileFormat.samplerate / 4);
    int arrayLength = frameCount * fileFormat.channels;
    short *buffer = new short[arrayLength];
    int iter = ceil((float)(fileFormat.frames) / (float)(frameCount));

    for (int i = 0; i < iter; i++) {
        if (nextSong != 0) break;
        if (seekTo != 0) {
            frame += seekTo;
            if (frame >= numFrames) frame = numFrames -1;
            if (frame < 0) frame = 0;
            sf_seek(file, frame, SF_SEEK_SET);
            seekTo = 0;
        }
        arrayLength = sf_read_short(file, buffer, arrayLength);
        drawGUI();
        ao_play(device, (char*)buffer, arrayLength * (format.bits / 8));
        frame += arrayLength / fileFormat.channels;
    }

    // done playing: close the file and the audio interface
    sf_close(file);
    ao_close(device);
}

int main(int argc, char *argv[]) {
    PlaylistParser lists = PlaylistParser();
    playlist = lists.getRoot();
    if (argc > 1) {
        std::string option = argv[1];
        if (option == "help") {
            std::cout << "Formatting key:\n() - a peice of raw text\n[] - an optional argument\ntext - arbitrary name for an argument\n\nList of commands:\n(help) - Report this list\n(versions) - Report various important versions, including the app version\n(categorize), folder[, playlist] - Load the contents of a folder into the local .playlists file\n(artist), name - Play all songs by a single artist\n(album), name - Play all songs in a single album\n(playlist), name - play all songs inside a playlist";
            return 0;
        }
        // load in an entire folder of songs as a playlist
        if (option == "categorize") {
            Playlist *list = lists.getRoot();
            if (argc >= 4) list = lists.getPlaylist(argv[3]);
            for (const auto &entry : std::filesystem::directory_iterator(argv[2]))
                lists.addFromPath((char *)entry.path().c_str(), list->id);
            lists.saveToDisk();
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
            ao_shutdown();
            runInputs = false;
            return 0;
        }
        if (option == "sort") {
            Playlist *list = lists.getRoot();
            if (argc >= 3) list = lists.getPlaylist(argv[2]);
            lists.sortPlaylist(list->id);
            lists.saveToDisk();
            return 0;
        }
        if (option == "shuffle") {
            Playlist *list = lists.getRoot();
            if (argc >= 3) list = lists.getPlaylist(argv[2]);
            lists.shufflePlaylist(list->id);
            lists.saveToDisk();
            return 0;
        }
        if (option == "artist") {
            playlist = lists.getArtist(argv[2]);
        } else if (option == "album") {
            playlist = lists.getAlbum(argv[2]);
        } else if (option == "playlist") {
            playlist = lists.getPlaylist(argv[2]);
        }
    }
    if (playlist->songs.size() <= 0) return 0;
    std::thread gui(inputProc);
    ao_initialize();
    driver = ao_default_driver_id();
    if (shuffle) lists.shufflePlaylist(playlist->id);
    do {
        if (!loop && playing >= playlist->songs.size()) break;
        title = playlist->songs[playing]->title;
        artist = playlist->songs[playing]->artist;
        playFile(playlist->songs[playing]->path, false);
        if (nextSong != 0) playing += nextSong;
        else playing++;
        if (playing < 0) playing = playlist->songs.size() -1;
        nextSong = 0;
        if (loop && playing >= playlist->songs.size()) {
            playing = 0;
            if (shuffle) lists.shufflePlaylist(playlist->id);
        }
    } while (true);
    runInputs = false;
    ao_shutdown();
}