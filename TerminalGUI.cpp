#include <thread>
#include <ncurses.h>
#include <vector>
#include <map>
#include <math.h>
#include <iostream>
#include <string>
#include <chrono>
#include <functional>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sdbus-c++/sdbus-c++.h>
#ifndef TERMINAL_LOADED
#define TERMINAL_LOADED
#include "./InterfaceGUI.cpp"
#include "./AudioPlayer.hh"
#include "./PlaylistParser.cpp"

class AudioPlayer;
class TerminalGUI : public InterfaceGUI {
    private:
        bool single = false;
        std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> start = std::chrono::system_clock::now();
        int timeToClose = -1;
        bool runInputs = true;
        int scrollAdvance = 0;
        std::thread gui;
        sdbus::IObject *remote;
        std::string namePlate;
        int originalPlaying;
        
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
            if (time < 0) return "--:--";
            std::string seconds = std::to_string(time % 60);
            std::string minutes = std::to_string((time / 60) % 60);
            std::string hours = std::to_string((time / 60) / 60);
            seconds = std::string(2 - seconds.length(), '0') + seconds;
            minutes = std::string(2 - minutes.length(), '0') + minutes;
            if (hours == "0") return minutes + ":" + seconds;
            return hours + ":" + minutes + ":" + seconds;
        }
        std::string framesToString(int frames) {
            return secondsToString((int)round((float)frames / (float)player->frameRate));
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

        void drawGUI() {
            winsize size = getTerminalSize();
        
            float prog = ((float)player->frame / (float)player->numFrames) * size.ws_col;
            int y = 0;
            scrollAdvance++;
            std::cout << "\x1b[0m";
            int songs = player->playlist->songs.size();
            int height = size.ws_row;
            for (int i = player->playing - (height / 2); i < songs && y < height; i++) {
                y++;
                std::cout << "\x1b[" << y << ";1H";
                if (i < 0) {
                    std::cout << "\x1b[2K";
                    continue;
                }
                std::string left = framesToString(player->frame);
                std::string right = framesToString(player->numFrames);
                if (i != player->playing) { left = ""; right = ""; }
                left += std::string(10.0 - left.length() <= 0 ? left.length() +1 : 10 - left.length(), ' ');
                right = std::string(10.0 - right.length() <= 0 ? right.length() +1 : 10 - right.length(), ' ') + right;
                std::string line;
                int wouldBeWidth = left.length() + right.length() + 5 + player->playlist->songs[i]->title.length() + player->playlist->songs[i]->artist.length();
                if (i == player->playing) std::cout << "\x1b[97m";
                else std::cout << "\x1b[90m";
                if (wouldBeWidth > size.ws_col && i == player->playing) {
                    line = left;

                    // draw the scrolling text banner
                    std::string scroller = ";  By: " + player->playlist->songs[i]->artist + "            " + player->playlist->songs[i]->title;
                    int scrollArea = size.ws_col - (left.length() + right.length());
                    int scrollLen = strLen(scroller);
                    int charIdx = (scrollAdvance % scrollLen);
                    bool opened = false;
                    for (int j = 0; j < scrollArea; j++) {
                        // caught int he middle of a multi-parter, skip out until we are not
                        if ((scroller[charIdx] & 0xC0) == 0x80 && !opened) {
                            charIdx++;
                            scrollAdvance++;
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
                    left += player->playlist->songs[i]->title;
                    int titleLen = strLen(player->playlist->songs[i]->title);
                    int artistLen = strLen(player->playlist->songs[i]->artist);
                    int leftLen = strLen(left);
                    if (((artistLen + 6 + right.length()) + leftLen) >= size.ws_col) {
                        if (leftLen > (size.ws_col / 2))
                            left = left.substr(0, (size.ws_col / 2) - 4) + "... ";
                        if ((artistLen + 6 + right.length()) > (size.ws_col / 2)) {
                            right = "By: " + player->playlist->songs[i]->artist.substr(0, (size.ws_col / 2) - (8 + right.length())) + "... " + right;
                            rightShrunk = true;
                        }
                    }
                    if (!rightShrunk) right = "By: " + player->playlist->songs[i]->artist + right;
                    line = left + std::string(size.ws_col - (strLen(right) + strLen(left)), ' ') + right;
                }
                bool hasFlipped = false;
                if (i == player->playing) std::cout << "\x1b[4m";
                int c = 0;
                for (int j = 0; j < line.length(); j++) {
                    std::cout << line[j];
                    if (c >= prog && !hasFlipped && (line[j] & 0b11000000) != 0b10000000 && !(line[j] & 0b10000000)) {
                        std::cout << "\x1b[24m";
                        hasFlipped = true;
                        continue;
                    }
                    if ((line[j] & 0b11000000) != 0b10000000) c++;
                }
                std::cout << "\x1b[24m";
            }

            if (pause) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) << ";1Hpaused\x1b[0m";
            if (loop) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) << ";" << (size.ws_col -3) << "Hloop\x1b[0m";
            if (height > 2) {
                if (timeToClose != -1) {
                    auto current = std::chrono::system_clock::now();
                    int since = (((current - start).count() / 1000) / 1000) / 1000;
                    if ((timeToClose - since) <= 0) {
                        exitApp = true;
                    } else {
                        std::string timeStamp = secondsToString(timeToClose - since);
                        std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) +2 << ";1H" << timeStamp << "\x1b[0m";
                    }
                }
                if (shuffle) std::cout << "\x1b[90m\x1b[3m\x1b[" << (height / 2) +2 << ";" << (size.ws_col -6) << "Hshuffle\x1b[0m";
            }
            std::cout << "\x1b[" << size.ws_row -1 << ":1H" << "\n";
        }
        // this gets run under a seperate thread due to weird blocking shenanigans
        void inputProc() {
            initscr();
            noecho();
            int stage = 0;
            runInputs = true;

            namePlate = "org.mpris.MediaPlayer2.AudioPlayer.I";
            namePlate += std::to_string(getpid());
            auto connection = sdbus::createDefaultBusConnection(namePlate);

            sdbus::ObjectPath objectPath{"/org/mpris/MediaPlayer2"};
            auto remotePntr = sdbus::createObject(*connection, std::move(objectPath));
            remote = remotePntr.get();

            // implements org.mpris.MediaPlayer2
            remotePntr->registerMethod("Raise")
                .onInterface("org.mpris.MediaPlayer2")
                .implementedAs([this]() {});
            remotePntr->registerMethod("Quit")
                .onInterface("org.mpris.MediaPlayer2")
                .implementedAs([this]() { exitApp = true; });
                
            remotePntr->registerProperty("CanQuit")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("Fullsceen")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return false; })
                .withSetter([this](bool value) {});
            remotePntr->registerProperty("CanSetFullscreen")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return false; });
            remotePntr->registerProperty("CanRaise")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return false; });
            remotePntr->registerProperty("HasTrackList")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return false; });
            remotePntr->registerProperty("Identity")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return "Audio Player"; });
            remotePntr->registerProperty("DesktopEntry")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() { return ""; });
            remotePntr->registerProperty("SupportedUriSchemes")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() {
                    std::vector<std::string> list;
                    // list.push_back("http");
                    list.push_back("file");
                    // list.push_back("data");
                    return list;
                });
            remotePntr->registerProperty("SupportedMimeTypes")
                .onInterface("org.mpris.MediaPlayer2")
                .withGetter([this]() {
                    std::vector<std::string> list;
                    // list.push_back("http");
                    list.push_back("audio/wav");
                    list.push_back("audio/aiff");
                    list.push_back("audio/pcm");
                    list.push_back("audio/flac");
                    list.push_back("audio/ogg");
                    list.push_back("audio/mp3");
                    return list;
                });

            // implements org.mpris.MediaPlayer2.Player
            remotePntr->registerMethod("Next")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    nextSong += 1;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    std::map<std::string, sdbus::Variant> newProperties;
                    newProperties["mpris:trackid"] = player->playlist->songs[player->playing + nextSong]->path;
                    newProperties["mpris:length"] = ((double)player->numFrames / player->frameRate) * 1000;
                    newProperties["xesam:artist"] = std::vector<std::string>{player->playlist->songs[player->playing + nextSong]->artist};
                    newProperties["xesam:album"] = player->playlist->songs[player->playing + nextSong]->album;
                    newProperties["xesam:title"] = player->playlist->songs[player->playing + nextSong]->title;
                    properties["Metadata"] = newProperties;
                    properties["Position"] = 0;
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("Previous")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    nextSong -= 1;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    std::map<std::string, sdbus::Variant> newProperties;
                    newProperties["mpris:trackid"] = player->playlist->songs[player->playing + nextSong]->path;
                    newProperties["mpris:length"] = ((double)player->numFrames / player->frameRate) * 1000;
                    newProperties["xesam:artist"] = std::vector<std::string>{player->playlist->songs[player->playing + nextSong]->artist};
                    newProperties["xesam:album"] = player->playlist->songs[player->playing + nextSong]->album;
                    newProperties["xesam:title"] = player->playlist->songs[player->playing + nextSong]->title;
                    properties["Metadata"] = newProperties;
                    properties["Position"] = 0;
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("Pause")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    pause = true;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["PlayStatus"] = "Playing";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("PlayPause")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    pause = !pause;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["PlayStatus"] = pause ? "Paused" : "Playing";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("Stop")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    seekTo = -player->frame;
                    pause = true;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["PlayStatus"] = "Stopped";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("Play")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this]() {
                    pause = false;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["PlayStatus"] = "Paused";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerMethod("Seek")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this](int Offset) {
                    seekTo = (((float)Offset / 1000) * player->frameRate);
                    auto signal = remote->createSignal("org.mpris.MediaPlayer2.Player", "Seeked");
                    signal << seekTo;
                    remote->emitSignal(signal);
                    auto changes = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    changes << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["Position"] = player->frame + seekTo;
                    changes << properties;
                    changes << std::vector<std::string>();
                    remote->emitSignal(changes);
                });
            remotePntr->registerMethod("SetPosition")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this](std::string TrackId, int Offset) {
                    if (TrackId != player->playlist->songs[player->playing]->path) return;
                    seekTo = (((float)Offset / 1000) * player->frameRate) - player->frame;
                    auto signal = remote->createSignal("org.mpris.MediaPlayer2.Player", "Seeked");
                    signal << seekTo;
                    remote->emitSignal(signal);
                    auto changes = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    changes << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["Position"] = player->frame + seekTo;
                    changes << properties;
                    changes << std::vector<std::string>();
                    remote->emitSignal(changes);
                });
            remotePntr->registerMethod("OpenUri")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .implementedAs([this](std::string Uri) {
                    int firstSlash = Uri.find('/');
                    int secondSlash = Uri.find('/', firstSlash);
                    int thirdSlash = Uri.find('/', secondSlash);
                    if ((secondSlash - firstSlash) > 1) secondSlash = firstSlash;
                    if ((thirdSlash - secondSlash) > 1) thirdSlash = secondSlash;
                    player->playing = player->playlist->songs.size();
                    player->lists->addFromPath((char *)Uri.substr(thirdSlash).c_str(), player->playlist->id);
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    std::map<std::string, sdbus::Variant> newProperties;
                    newProperties["mpris:trackid"] = player->playlist->songs[player->playing + nextSong]->path;
                    newProperties["mpris:length"] = ((double)player->numFrames / player->frameRate) * 1000;
                    newProperties["xesam:artist"] = std::vector<std::string>{player->playlist->songs[player->playing]->artist};
                    newProperties["xesam:album"] = player->playlist->songs[player->playing + nextSong]->album;
                    newProperties["xesam:title"] = player->playlist->songs[player->playing + nextSong]->title;
                    properties["Metadata"] = newProperties;
                    properties["Position"] = 0;
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });

            remotePntr->registerSignal("Seeked")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withParameters<int>("Time_In_Us");

            remotePntr->registerProperty("PlaybackStatus")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return pause ? "Paused" : "Playing"; });
            remotePntr->registerProperty("LoopStatus")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return loop ? "Playlist" : "None"; })
                .withSetter([this](std::string value) {
                    loop = value == "Playlist";
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["LoopStatus"] = loop ? "Playlist" : "None";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerProperty("Rate")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return 1.0; })
                .withSetter([this](double value) {});
            remotePntr->registerProperty("Shuffle")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return shuffle; })
                .withSetter([this](bool value) {
                    shuffle = value;
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    std::map<std::string, sdbus::Variant> properties;
                    properties["Shuffle"] = shuffle;
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                });
            remotePntr->registerProperty("Metadata")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() {
                    std::map<std::string, sdbus::Variant> properties;
                    properties["mpris:trackid"] = player->playlist->songs[player->playing + nextSong]->path;
                    properties["mpris:length"] = ((double)player->numFrames / player->frameRate) * 1000;
                    properties["xesam:artist"] = std::vector<std::string>{player->playlist->songs[player->playing + nextSong]->artist};
                    properties["xesam:album"] = player->playlist->songs[player->playing + nextSong]->album;
                    properties["xesam:title"] = player->playlist->songs[player->playing + nextSong]->title;
                    return properties;
                });
            remotePntr->registerProperty("Volume")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return 1; })
                .withSetter([this](double value) {});
            remotePntr->registerProperty("Position")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return ((double)player->frame / player->frameRate) * 1000; });
            remotePntr->registerProperty("MinimumRate")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return 1.0; });
            remotePntr->registerProperty("MaximumRate")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return 1.0; });
            remotePntr->registerProperty("CanGoNext")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("CanGoPrevious")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("CanPlay")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("CanPause")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("CanSeek")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->registerProperty("CanControl")
                .onInterface("org.mpris.MediaPlayer2.Player")
                .withGetter([this]() { return true; });
            remotePntr->finishRegistration();

            connection->enterEventLoopAsync();
            while (runInputs) {
                timeout(250);
                int key = getch();
                if (stage > 2) stage = 0;
                std::map<std::string, sdbus::Variant> properties;
                if (key != -1) {
                    switch (stage) {
                    case 0:
                        switch (key) {
                        case '\x1b':
                            stage = 1;
                            break;
                        case 'l':
                            loop = !loop;
                            properties["LoopStatus"] = loop ? "Playlist" : "None";
                            break;
                        case ' ':
                            pause = !pause;
                            properties["PlaybackStatus"] = pause ? "Paused" : "Playing";
                            break;
                        case 's': {
                            Song *currentSong = player->playlist->songs[player->playing];
                            lists->shufflePlaylist(player->playlist->id);
                            shuffle = true;
                            properties["Shuffle"] = shuffle;
                            for (int i = 0; i < player->playlist->songs.size(); i++) {
                                if (player->playlist->songs[i] == currentSong) {
                                    player->playing = i;
                                    break;
                                }
                            }
                            break;
                        }
                        case 'S': {
                            Song *currentSong = player->playlist->songs[player->playing];
                            lists->sortPlaylist(player->playlist->id);
                            shuffle = false;
                            properties["Shuffle"] = shuffle;
                            for (int i = 0; i < player->playlist->songs.size(); i++) {
                                if (player->playlist->songs[i] == currentSong) {
                                    player->playing = i;
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
                            start = std::chrono::system_clock::now();
                            break;
                        }
                        break;
                    case 1:
                        stage = 2;
                        break;
                    case 2:
                        switch (key) {
                        case 'A': nextSong--; break;
                        case 'B': nextSong++; break;
                        case 'C': seekTo += player->frameRate; break;
                        case 'D': seekTo -= player->frameRate; break;
                        }
                        stage = 0;
                        break;
                    }
                }
                if (nextSong != 0 || player->playing != originalPlaying) {
                    originalPlaying = player->playing;
                    std::map<std::string, sdbus::Variant> newProperties;
                    newProperties["mpris:trackid"] = player->playlist->songs[player->playing + nextSong]->path;
                    newProperties["mpris:length"] = ((double)player->numFrames / player->frameRate) * 1000;
                    newProperties["xesam:artist"] = std::vector<std::string>{player->playlist->songs[player->playing + nextSong]->artist};
                    newProperties["xesam:album"] = player->playlist->songs[player->playing + nextSong]->album;
                    newProperties["xesam:title"] = player->playlist->songs[player->playing + nextSong]->title;
                    properties["Metadata"] = newProperties;
                    properties["Position"] = 0;
                }
                if (seekTo != 0) {
                    auto signal = remote->createSignal("org.mpris.MediaPlayer2.Player", "Seeked");
                    signal << seekTo;
                    remote->emitSignal(signal);
                }
                if (properties.size() > 0) {
                    auto signal = remote->createSignal("org.freedesktop.DBus.Properties", "PropertiesChanged");
                    signal << "org.mpris.MediaPlayer2.Player";
                    signal << properties;
                    signal << std::vector<std::string>();
                    remote->emitSignal(signal);
                }
                drawGUI();
            }
            endwin();
        }
    public:
        TerminalGUI(AudioPlayer *player, PlaylistParser *lists) {
            gui = std::thread(std::bind(&TerminalGUI::inputProc, this));
            this->player = player;
            this->lists = lists;
        }
        ~TerminalGUI() {
            runInputs = false;
            gui.join();

        }
};
#endif