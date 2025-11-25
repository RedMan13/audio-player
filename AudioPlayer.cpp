#include <ao/ao.h>
#include <iostream>
#include <string>
#include <sndfile.h>
#include <math.h>
#include <sys/ioctl.h>

// max buffer size, normally the buffer will ensure the file is devided into one second 
// chunks, except if the requested size excedes this limit
#define MAX_BUFFER 16777216
int driver;
// not actual frames!!!!
// these are the frames as defined by sndfile, as in samples
int numFrames;
int frame;
int frameRate;
std::string title;
std::string artist;
std::string playlist[];

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

    float per = (float)frame / (float)numFrames;
    std::string left = framesToString(frame) + " " + title;
    std::string right = "By: " + artist + " " + framesToString(numFrames);
    std::cout << "\x1b[1;1H\x1b[2K";
    std::cout << left;
    std::cout << "\x1b[1;" << (size.ws_col - right.length()) +1 << "H";
    std::cout << right << "\n";
    std::cout << std::string((int)((size.ws_col +1) * per), '=');
    std::cout << std::string((int)((size.ws_col +1) - (size.ws_col * per)), '-');
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
    artist = sf_get_string(file, SF_STR_ARTIST);
    title = sf_get_string(file, SF_STR_TITLE);
    // manually pipe data between sndfile and ao
    int frameCount = fileFormat.samplerate > MAX_BUFFER ? MAX_BUFFER : fileFormat.samplerate;
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

int main() {
    ao_initialize();
    title = sf_version_string();
    driver = ao_default_driver_id();
    playFile("./bad-apple.wav");
    ao_shutdown();
}