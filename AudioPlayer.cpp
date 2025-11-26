#include <ao/ao.h>
#include <iostream>
#include <string>
#include <sndfile.h>
#include <math.h>
#include <sys/ioctl.h>
#include <vector>

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
std::vector<char *> playlist;
bool loop = true;
bool shuffle = true;

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
    std::cout << "\x1b[0m\x1b[2K";
    if ((left.length() + right.length()) > size.ws_col) {
        left = framesToString(frame) + " ";
        right = " " + framesToString(numFrames);

        // draw the scrolling text banner
        std::string scroller = title + "; By: " + artist + "      ";
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
    std::cout << right << "\n\x1b[97m";
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
    for (int i = 0; i < playlist.size(); i++) {
        if (playing == i) std::cout << "\x1b[97m";
        else std::cout << "\x1b[90m";
        std::cout << "\n" << playlist[i];
    }
}

void playFile(char *fileName) {
    SF_INFO fileFormat;
    SNDFILE *file = sf_open(fileName, SFM_READ, &fileFormat);
    if (sf_error(file) > 0) {
        std::cerr << sf_strerror(file);
        return;
    }

    // make format metadata for ao
    ao_sample_format format;
    format.bits = 16;
    format.rate = fileFormat.samplerate;
    format.channels = fileFormat.channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = (char*)"L,R";
    ao_device *device = ao_open_live(driver, &format, NULL);
    if (device == NULL) {
        std::cerr << "Error opening device. " << errno << " ";
        switch (errno) {
        case AO_ENODRIVER: std::cerr << "No driver could be found under " << driver; break;
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
    if (artistChars == NULL) artist = "Unknown";
    else artist = artistChars;
    if (titleChars == NULL) title = fileName;
    else title = titleChars;
    // manually pipe data between sndfile and ao
    int frameCount = (fileFormat.samplerate / 2) > MAX_BUFFER ? MAX_BUFFER : (fileFormat.samplerate / 2);
    int arrayLength = frameCount * fileFormat.channels;
    short *buffer = new short[arrayLength];
    int iter = ceil((float)(fileFormat.frames) / (float)(frameCount));

    for (int i = 0; i < iter; i++) {
        arrayLength = sf_read_short(file, buffer, arrayLength);
        frame += arrayLength / fileFormat.channels;
        drawGUI();
        ao_play(device, (char*)buffer, arrayLength * (format.bits / 8));
    }

    // done playing: close the file and the audio interface
    sf_close(file);
    ao_close(device);
}

void shufflePlaylist() {
    for (int i = 0; i < playlist.size(); i++) {
        int target = ((double)random() / 2147483647.0) * playlist.size();
        auto temp = playlist[target];
        playlist[target] = playlist[target];
        playlist[target] = temp;
    }
}

int main(int argc, char *argv[]) {
    ao_initialize();
    driver = ao_default_driver_id();
    playlist.push_back("./trainrolling.wav");
    playlist.push_back("./bad-apple.wav");
    if (shuffle) shufflePlaylist();
    do {
        playFile(playlist[playing]);
        playing++;
        if (!loop && playing >= playlist.size()) break;
        if (loop && playing >= playlist.size()) {
            playing = 0;
            if (shuffle) shufflePlaylist();
        }
    } while (true);
    ao_shutdown();
}