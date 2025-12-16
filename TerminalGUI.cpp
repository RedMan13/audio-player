#include <thread>
#include <ncurses.h>
#include <vector>
#include <math.h>
#include <iostream>
#include <string>
#include <chrono>
#include <functional>
#include <sys/ioctl.h>
#ifndef TERMINAL_LOADED
#define TERMINAL_LOADED
#include "./InterfaceGUI.cpp"
#include "./AudioPlayer.hh"
#include "./PlaylistParser.cpp"

class AudioPlayer;
class TerminalGUI : public InterfaceGUI {
    private:
        bool single = false;
        std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> start = std::chrono::high_resolution_clock::now();
        int timeToClose = -1;
        bool runInputs = true;
        int scrollAdvance = 0;
        std::thread gui;
        
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
                    auto current = std::chrono::high_resolution_clock::now();
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
                        Song *currentSong = player->playlist->songs[player->playing];
                        lists->shufflePlaylist(player->playlist->id);
                        shuffle = true;
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
                        start = std::chrono::high_resolution_clock::now();
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
                    case 'C':
                        seekTo += player->frameRate;
                        break;
                    case 'D':
                        seekTo -= player->frameRate;
                        break;
                    }
                    stage = 0;
                    break;
                }
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