#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/sync.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include <iostream>
#include <string>
#include <chrono>
#include <fontconfig/fontconfig.h>
#ifndef WINDOW_LOADED
#define WINDOW_LOADED
#include "./InterfaceGUI.cpp"
#include "./AudioPlayer.hh"

class XorgGUI : public InterfaceGUI {
    private:
        std::thread gui;
        bool single = false;
        std::chrono::_V2::system_clock::time_point start = std::chrono::high_resolution_clock::now();
        int timeToClose = -1;
        bool runInputs = true;
        int scrollAdvance = 0;
        bool runWindow = true;
        GC context;
        Window rootWindow;
        Display *mainDisplay;
        Window mainWindow;
        Pixmap textPlate;
        XftDraw *xftdraw;
        bool textNeedsDraw = true;
        int windowWidth;
        int windowHeight;
        int windowDepth;
        int mouseX = 0;
        int mouseY = 0;
        int timeLine = 0;
        std::chrono::_V2::system_clock::time_point startHover = std::chrono::high_resolution_clock::now();
        int showingFrame = 0;
        int wasPlaying = 0;
        float delta = 0;
        XftFont *termFont;
        std::chrono::_V2::system_clock::time_point frameStart = std::chrono::high_resolution_clock::now();

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
        void drawText() {
            XClearWindow(mainDisplay, mainWindow);
            XGCValues values = {};
            values.line_width = 1;
            XChangeGC(mainDisplay, context, GCLineWidth, &values);
            if (player->playing != wasPlaying) {
                wasPlaying = player->playing;
                showingFrame = 0;
                textNeedsDraw = true;
            }
            if (player->buffer != NULL) {
                int chunkLen = player->frameRate * delta;
                if (showingFrame < player->frame) showingFrame = player->frame;
                int bufferStart = showingFrame - player->frame;
                for (int j = 0; j < player->channels; j++) {
                    int bufferOffset = bufferStart + j;
                    XPoint *points = new XPoint[chunkLen];
                    for (int i = 0; i < chunkLen; i++) {
                        bufferOffset += 2;
                        points[i].x = ((float)i / chunkLen) * windowWidth;
                        points[i].y = windowHeight / 2;
                        if (bufferOffset < 0) continue;
                        if (bufferOffset > player->arrayLength) continue;
                        points[i].y = ((32767 + (float)player->buffer[bufferOffset]) / 0xFFFF) * windowHeight;
                    }
                    XDrawLines(mainDisplay, mainWindow, context, points, chunkLen, CoordModeOrigin);
                }
                if (!pause) showingFrame += chunkLen;
            }
            values.function = GXinvert;
            XChangeGC(mainDisplay, context, GCFunction, &values);
            
            int y = 0;
            scrollAdvance++;
            int songs = player->playlist->songs.size();
            XRenderColor xrcolor;
            XftColor xftcolor;
            xrcolor.red = 0;
            xrcolor.green = 0;
            xrcolor.blue = 0;
            xrcolor.alpha = 65535;
            XftColorAllocValue(mainDisplay, DefaultVisual(mainDisplay, 0), DefaultColormap(mainDisplay, 0), &xrcolor, &xftcolor);
            int charWidth = termFont->max_advance_width;
            int charHeight = termFont->height;
            int width = windowWidth / charWidth;
            int height = windowHeight / charHeight;
            for (int i = player->playing - (height / 2); i < songs && y < windowHeight; i++) {
                y += charHeight;
                if (i < 0) continue;
                std::string left = framesToString(player->frame);
                std::string right = framesToString(player->numFrames);
                if (i != player->playing) { left = ""; right = ""; }
                left += std::string(10.0 - left.length() <= 0 ? left.length() +1 : 10 - left.length(), ' ');
                right = std::string(10.0 - right.length() <= 0 ? right.length() +1 : 10 - right.length(), ' ') + right;
                std::string line;
                int wouldBeWidth = left.length() + right.length() + 5 + player->playlist->songs[i]->title.length() + player->playlist->songs[i]->artist.length();
                // if (i == player->playing) std::cout << "\x1b[97m";
                // else std::cout << "\x1b[90m";
                if (wouldBeWidth > width && i == player->playing) {
                    line = left;

                    // draw the scrolling text banner
                    std::string scroller = ";  By: " + player->playlist->songs[i]->artist + "            " + player->playlist->songs[i]->title;
                    int scrollArea = width - (left.length() + right.length());
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
                    left += player->playlist->songs[i]->title;
                    int titleLen = strLen(player->playlist->songs[i]->title);
                    int artistLen = strLen(player->playlist->songs[i]->artist);
                    int leftLen = strLen(left);
                    if (((artistLen + 6 + right.length()) + leftLen) >= width) {
                        if (leftLen > (width / 2))
                            left = left.substr(0, (width / 2) - 4) + "... ";
                        if ((artistLen + 6 + right.length()) > (width / 2)) {
                            right = "By: " + player->playlist->songs[i]->artist.substr(0, (width / 2) - (8 + right.length())) + "... " + right;
                            rightShrunk = true;
                        }
                    }
                    if (!rightShrunk) right = "By: " + player->playlist->songs[i]->artist + right;
                    int padding = width - (strLen(right) + strLen(left));
                    if (padding < 0) padding = 0;
                    line = left + std::string(padding, ' ') + right;
                }
                XftDrawStringUtf8(xftdraw, &xftcolor, termFont, 0,y, (unsigned char*)line.c_str(), line.length());
                if (i == player->playing)
                    timeLine = y;
            }
            if (pause) XftDrawString8(xftdraw, &xftcolor, termFont, 0, timeLine - charHeight, (unsigned char*)"paused", 6);
            if (loop) XftDrawString8(xftdraw, &xftcolor, termFont, (width -4) * charWidth, timeLine - charHeight, (unsigned char*)"loop", 4);
            if (height > 2) {
                if (timeToClose != -1) {
                    auto current = std::chrono::high_resolution_clock::now();
                    int since = (((current - start).count() / 1000) / 1000) / 1000;
                    if ((timeToClose - since) <= 0) {
                        exitApp = true;
                    } else {
                        std::string timeStamp = secondsToString(timeToClose - since);
                        XftDrawString8(xftdraw, &xftcolor, termFont, 0, timeLine + charHeight, (unsigned char*)timeStamp.c_str(), timeStamp.length());
                    }
                }
                if (shuffle) XftDrawString8(xftdraw, &xftcolor, termFont, (width -7) * charWidth, timeLine + charHeight, (unsigned char*)"shuffle", 7);
            }

            XftColorFree(mainDisplay, DefaultVisual(mainDisplay, 0),DefaultColormap(mainDisplay, 0), &xftcolor);
            values.line_width = 2;
            values.function = GXcopy;
            XChangeGC(mainDisplay, context, GCLineWidth | GCFunction, &values);
            XDrawLine(mainDisplay, mainWindow, context, 0, timeLine +2, windowWidth * ((float)player->frame / player->numFrames), timeLine +2);
            XFlush(mainDisplay);
        }
        void eventLoop() {
            mainDisplay = XOpenDisplay(0);
            rootWindow = XDefaultRootWindow(mainDisplay);

            int defaultScreen = DefaultScreen(mainDisplay);
            context = XDefaultGC(mainDisplay, defaultScreen);
            
            int windowX = 0;
            int windowY = 0;
            windowWidth = 816;
            windowHeight = 481;
            int borderWidth = 1;
            windowDepth = 24;
            int windowClass = CopyFromParent;
            Visual* windowVisual = CopyFromParent;

            int attributeValueMask = CWBackPixel | CWEventMask;
            XSetWindowAttributes windowAttributes = {};
            windowAttributes.background_pixel = 0xffffeeaa;
            windowAttributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ExposureMask | PointerMotionMask;

            mainWindow = XCreateWindow(mainDisplay, rootWindow, 
                    windowX, windowY, windowWidth, windowHeight,
                    borderWidth, windowDepth, windowClass, windowVisual,
                    attributeValueMask, &windowAttributes);
                
            XMapWindow(mainDisplay, mainWindow);
            XStoreName(mainDisplay, mainWindow, "Audio Player");
            int sync_event, sync_error, tmp;
            if (!XSyncQueryExtension(mainDisplay, &sync_event, &sync_error))
                std::cerr << "Could not enable the sync extension";
            if (!XSyncInitialize(mainDisplay, &tmp, &tmp))
                std::cerr << "Could not initialize the sync extension";
            int ncounter;
            XSyncSystemCounter *counters;
            XSyncCounter servertime = None;

            if ((counters = XSyncListSystemCounters(mainDisplay, &ncounter)) != NULL) {
                for (int i = 0; i < ncounter; i++) {
                    if ((std::string)counters[i].name == "SERVERTIME") {
                        servertime = counters[i].counter;
                        break;
                    }
                }
                XSyncFreeSystemCounterList(counters);
            }
            if (servertime == None) std::cerr << "Missing the server time counter";

            XSyncAlarmAttributes attr;
            unsigned long flags = 0;
            attr.trigger.counter = servertime;
            flags |= XSyncCACounter;
            XSyncIntToValue(&attr.trigger.wait_value, 16);
            flags |= XSyncCAValue;
            attr.trigger.value_type = XSyncRelative;
            flags |= XSyncCAValueType;
            attr.trigger.test_type = XSyncPositiveComparison;
            flags |= XSyncCATestType;
            XSyncIntToValue(&attr.delta, 16);
            flags |= XSyncCADelta;

            xftdraw = XftDrawCreate(mainDisplay, mainWindow, DefaultVisual(mainDisplay, 0), DefaultColormap(mainDisplay, 0));
            termFont = XftFontOpen(mainDisplay, DefaultScreen(mainDisplay), XFT_FAMILY, XftTypeString, "Fira Mono", XFT_SIZE, XftTypeDouble, 10.0, NULL);
            XSyncAlarm alarm = XSyncCreateAlarm(mainDisplay, flags, &attr);
            while (runWindow) {
                XEvent generalEvent = {};
                XNextEvent(mainDisplay, &generalEvent);
                auto frameEnd = std::chrono::high_resolution_clock::now();
                delta = (float)(frameEnd - frameStart).count() / 1000000000;
                frameStart = std::chrono::high_resolution_clock::now();

                switch(generalEvent.type) {
                case Expose: {
                    XExposeEvent *event = (XExposeEvent *)&generalEvent;
                    windowWidth = event->width;
                    windowHeight = event->height;
                    break;
                }
                // case KeyPress:
                case KeyRelease: {
                    XKeyPressedEvent *event = (XKeyPressedEvent *)&generalEvent;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_Down)) 
                        nextSong++;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_Up)) 
                        nextSong--;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_Left)) 
                        seekTo -= player->frameRate;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_Right)) 
                        seekTo += player->frameRate;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_space)) 
                        pause = !pause;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_l)) 
                        loop = !loop;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_t)) 
                        seekTo += player->frameRate;
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_s)) {
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
                    if (event->keycode == XKeysymToKeycode(mainDisplay, XK_S)) {
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
                    break;
                }
                case MotionNotify: {
                    XMotionEvent *event = (XMotionEvent *)&generalEvent;
                    mouseX = event->x;
                    mouseY = event->y;
                    startHover = std::chrono::high_resolution_clock::now();
                    XSyncChangeAlarm(mainDisplay, alarm, flags, &attr);
                    break;
                }
                }
                drawText();
            }
        }
    public:
        XorgGUI(AudioPlayer *player, PlaylistParser *lists) {
            gui = std::thread(std::bind(&XorgGUI::eventLoop, this));
            this->player = player;
            this->lists = lists;
        }
        ~XorgGUI() {
            runWindow = false;
            gui.join();
        }
};
#endif