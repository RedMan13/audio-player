#ifndef WINDOW_LOADED
#define WINDOW_LOADED
#include "./InterfaceGUI.cpp"
#include "./AudioPlayer.hh"

class GLGUI : public InterfaceGUI {
    private:
        /* data */
    public:
        GLGUI(AudioPlayer *player, PlaylistParser *lists) {
            this->player = player;
            this->lists = lists;
        };
        ~GLGUI() {};
};
#endif