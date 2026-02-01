# Audio Player
simply a light weight cli-based audio player, like spotify but objectively better in every way but the interface.

pre-built files can be found at [`/dist`](<https://github.com/RedMan13/audio-player/tree/main/dist>) for your system architecture, mostly

building the codebase is as simple as running
```sh
./build-all.sh
```
if this fails due to `libsdbus` missing, just remove `-lsdbus-c++` from `build-local.sh` and optionally also `./build-all.sh`.

local builds will be under `dist/play` and publicly published builds will be under `dist/{arch}/play`

## Known bugs
- [x] ~~Audio chunks play twice on app starth~~
- [x] ~~Scrolling to fast can cause `ao_play` to get stuck~~
- [x] ~~Keys wont act after using control sequences like up down left and right~~
- [x] ~~UTF8 encoded text will artifact as the play bar passes by extended characters~~
- [x] ~~Audio will crackle and pop on some samples, mainly noticable with strong base~~
- [x] ~~Audio will tick after each chunk~~
- [x] ~~Playback of audio files uses all of a single CPU core~~