# Audio Player
simply a light weight cli-based audio player, like spotify but objectively better in every way but the interface.

## Known bugs
- [x] ~~Audio chunks play twice on app starth~~
- [ ] Scrolling to fast can cause `ao_play` to get stuck
- [x] ~~Keys wont act after using control sequences like up down left and right~~
- [ ] UTF8 encoded text will artifact as the play bar passes by extended characters
- [ ] Audio will crackle and pop on some samples, mainly noticable with strong base
- [ ] Playback of audio files uses all of a single CPU core