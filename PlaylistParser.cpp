#include <sndfile.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <ext/stdio_filebuf.h>
#ifndef PARSER_LOADED
#define PARSER_LOADED

struct Song {
    std::string path;
    std::string artist;
    std::string album;
    std::string title;
    std::string iconUrl;
};
struct Playlist {
    int id;
    std::string name;
    std::vector<Song*> songs;
    Playlist *next;
};

class PlaylistParser {
    private:
        // per playlist
        Playlist *playlists;
        // per artist
        Playlist *artists;
        // per album
        Playlist *albums;
        int idIncr;
        std::string esc(std::string str, bool strict) {
            std::string out = "";
            if (str[0] == ' ') out += '\\';
            for (int i = 0; i < str.length(); i++) {
                if (strict) {
                    switch (str[i]) {
                    case ';':
                    case ':':
                    case '\\':
                        out += '\\';
                        break;
                    }
                }
                out += str[i];
            }

            return out;
        }
        const std::string sortOrder = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ";
        bool textNeedsSwap(std::string a, std::string b) {
            int len = a.length();
            if (b.length() < len) len = b.length();
            for (int i = 0; i < len; i++) {
                int aIdx = sortOrder.find(a[i]);
                int bIdx = sortOrder.find(b[i]);
                if (aIdx == bIdx) continue;
                return aIdx > bIdx;
            }
            return false;
        }
        std::string getConfigPath() {
            std::string root = getenv("HOME");
            return root + "/.playlists";
        }
    public:
        PlaylistParser();
        Playlist *getRoot() { return playlists; }
        Playlist *getPlaylist(std::string name) {
            Playlist *end = playlists;
            Playlist *list = playlists;
            while (list != NULL) {
                if (list->name == name) return list;
                end = list;
                list = list->next;
            }
            
            Playlist *playlist = new Playlist();
            playlist->id = idIncr++;
            playlist->name = name;
            if (end == NULL) playlists = playlist;
            else end->next = playlist;
            return playlist;
        }
        Playlist *getPlaylist(int id) {
            Playlist *list = playlists;
            while (list != NULL) {
                if (list->id == id) return list;
                list = list->next;
            }
            
            return NULL;
        }
        Playlist *getAlbum(std::string name) {
            Playlist *end = albums;
            Playlist *list = albums;
            while (list != NULL) {
                if (list->name == name) return list;
                end = list;
                list = list->next;
            }
            
            Playlist *playlist = new Playlist();
            playlist->id = idIncr++;
            playlist->name = name;
            if (end == NULL) albums = playlist;
            else end->next = playlist;
            return playlist;
        }
        Playlist *getAlbum(int id) {
            Playlist *list = albums;
            while (list != NULL) {
                if (list->id == id) return list;
                list = list->next;
            }
            
            return NULL;
        }
        Playlist *getArtist(std::string name) {
            Playlist *end = artists;
            Playlist *list = artists;
            while (list != NULL) {
                if (list->name == name) return list;
                end = list;
                list = list->next;
            }
            
            Playlist *playlist = new Playlist();
            playlist->id = idIncr++;
            playlist->name = name;
            if (end == NULL) artists = playlist;
            else end->next = playlist;
            return playlist;
        }
        Playlist *getArtist(int id) {
            Playlist *list = artists;
            while (list != NULL) {
                if (list->id == id) return list;
                list = list->next;
            }
            
            return NULL;
        }
        Song *getSong(std::string path) {
            for (int i = 0; i < playlists->songs.size(); i++)
                if (playlists->songs[i]->path == path)
                    return playlists->songs[i];
            
            return NULL;
        }
        void sortPlaylist(int id) {
            Playlist *playlist = getPlaylist(id);
            for (int i = 0; i < playlist->songs.size(); i++) {
                for (int i = 0; i < playlist->songs.size() -1; i++) {
                    if (!textNeedsSwap(playlist->songs[i]->title, playlist->songs[i +1]->title)) continue;
                    auto temp = playlist->songs[i];
                    playlist->songs[i] = playlist->songs[i +1];
                    playlist->songs[i +1] = temp;
                }
            }
        }
        void sortPlaylist(std::string name) {
            Playlist *playlist = getPlaylist(name);
            for (int i = 0; i < playlist->songs.size(); i++) {
                for (int i = 0; i < playlist->songs.size() -1; i++) {
                    if (!textNeedsSwap(playlist->songs[i]->title, playlist->songs[i +1]->title)) continue;
                    auto temp = playlist->songs[i];
                    playlist->songs[i] = playlist->songs[i +1];
                    playlist->songs[i +1] = temp;
                }
            }
        }
        void sortPlaylist(Playlist *playlist) {
            for (int i = 0; i < playlist->songs.size(); i++) {
                for (int i = 0; i < playlist->songs.size() -1; i++) {
                    if (!textNeedsSwap(playlist->songs[i]->title, playlist->songs[i +1]->title)) continue;
                    auto temp = playlist->songs[i];
                    playlist->songs[i] = playlist->songs[i +1];
                    playlist->songs[i +1] = temp;
                }
            }
        }
        void shufflePlaylist(int id) {
            Playlist *playlist = getPlaylist(id);
            for (int i = 0; i < playlist->songs.size(); i++) {
                int target = ((double)random() / 2147483647.0) * playlist->songs.size();
                auto temp = playlist->songs[i];
                playlist->songs[i] = playlist->songs[target];
                playlist->songs[target] = temp;
            }
        }
        void shufflePlaylist(std::string name) {
            Playlist *playlist = getPlaylist(name);
            for (int i = 0; i < playlist->songs.size(); i++) {
                int target = ((double)random() / 2147483647.0) * playlist->songs.size();
                auto temp = playlist->songs[i];
                playlist->songs[i] = playlist->songs[target];
                playlist->songs[target] = temp;
            }
        }
        void shufflePlaylist(Playlist *playlist) {
            for (int i = 0; i < playlist->songs.size(); i++) {
                int target = ((double)random() / 2147483647.0) * playlist->songs.size();
                auto temp = playlist->songs[i];
                playlist->songs[i] = playlist->songs[target];
                playlist->songs[target] = temp;
            }
        }

        std::string extractArt(char *path) {
            if (getSong(path)) return getSong(path)->iconUrl;
            std::string name = path;
            name = name.substr(name.find_last_of('/') +1, name.find_last_of('.') -2);
            std::ifstream data(path);
            unsigned char header[11];
            data.read((char *)header, 10);
            if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
                int metaLength = (header[6] << 21) | (header[7] << 14) | (header[8] << 7) | header[9];
                int idx = 10;
                if (header[5] & 0b01000000) {
                    data.read((char *)header, 4);
                    int extSize = (header[0] << 21) | (header[1] << 14) | (header[2] << 7) | header[3];
                    idx += extSize;
                }
                do {
                    data.seekg(idx); // force over to where idx points
                    data.read((char *)header, 11);
                    idx += 10;
                    int tag = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
                    int size = (header[4] << 24) | (header[5] << 16) | (header[6] << 8) | header[7];
                    // short flags = (header[8] << 8) | header[9];
                    if (tag != 'APIC') { idx += size; continue; }
                    char imageData[size];
                    data.read(imageData, size);
                    bool littleEndian = false;
                    int firstZeros = 1;
                    if (header[10] > 0 && header[10] < 3) firstZeros = 2;
                    int lastZeros = firstZeros + 2;
                    std::string mimeType = "";
                    int i = 0;
                    for (int c = 0; i < size; i++) {
                        if (c == firstZeros) {
                            c++; // this way we are no LONGER first
                            if (imageData[i] > 0 && imageData[i] < 3) lastZeros = firstZeros + 3;
                            continue; // dont let the following treat this as null-term
                        }
                        if (imageData[i] == 0) c++;
                        if (c >= lastZeros) break;
                        // convert all strings back to utf8, since its betterer
                        if (c < firstZeros) {
                            switch (header[10]) {
                            case 0x01: if (i == 0) littleEndian = imageData[0] == 0xFF;
                            case 0x02: {
                                if (i % 2) break;
                                short firstCodePoint = littleEndian
                                    ? (imageData[i +1] << 8) | imageData[i]
                                    : (imageData[i] << 8) | imageData[i +1];
                                short secondCodePoint = littleEndian
                                    ? (imageData[i +1] << 8) | imageData[i]
                                    : (imageData[i] << 8) | imageData[i +1];
                                int codePoint = firstCodePoint;
                                if ((firstCodePoint & 0xFF00) >= 0xD800 && (secondCodePoint & 0xFF00) >= 0xDC00)
                                    codePoint = ((firstCodePoint - 0xD800) << 10) | (secondCodePoint - 0xDC00);
                                if (codePoint <= 0x7F) mimeType += (char)secondCodePoint; 
                                else if (codePoint <= 0x7FF) {
                                    mimeType += (char)(0b11000000 | ((codePoint & 0b11111000000) >> 6));
                                    mimeType += (char)(0b10000000 | (codePoint & 0b00000111111));
                                } else if (codePoint <= 0xFFFF) {
                                    mimeType += (char)(0b11100000 | ((codePoint & 0b1111000000000000) >> 12));
                                    mimeType += (char)(0b10000000 | ((codePoint & 0b0000111111000000) >> 6));
                                    mimeType += (char)(0b10000000 | (codePoint & 0b0000000000111111));
                                } else { // anything larger *cant* exist in either format, so even if we somehow get that it will need trimmed anyways
                                    mimeType += (char)(0b11110000 | ((codePoint & 0b111000000000000000000) >> 18));
                                    mimeType += (char)(0b10000000 | ((codePoint & 0b000111111000000000000) >> 12));
                                    mimeType += (char)(0b10000000 | ((codePoint & 0b000000000111111000000) >> 6));
                                    mimeType += (char)(0b10000000 | (codePoint & 0b000000000000000111111));
                                }
                                break;
                            }
                            case 0x00:
                                if (imageData[i] > 0x7F) {
                                    mimeType += (char)(0b11000000 | ((imageData[i] & 0b11000000) >> 6));
                                    mimeType += (char)(0b10000000 | (imageData[i] & 0b00111111));
                                    break;
                                }
                                mimeType += imageData[i];
                                break;
                            default:
                            case 0x03: mimeType += imageData[i]; break;
                            }
                        }
                    }
                    std::string extension = ".bin";
                    if (mimeType == "image/png") extension = ".png";
                    else if (mimeType == "image/jpeg") extension = ".jpg";
                    else if (mimeType == "image/webp") extension = ".webp";
                    else if (mimeType == "image/gif") extension = ".gif";
                    else if (mimeType == "image/svg+xml") extension = "svg";
                    else if (mimeType == "image/bmp") extension = ".bmp";
                    else if (mimeType == "image/avif") extension = ".avif";
                    else if (mimeType == "image/tiff") extension = ".tiff";
                    std::string folder = path;
                    std::string iconPath = folder.substr(0, folder.find_last_of('/') +1) + '/' + name + extension;
                    std::ofstream output(iconPath);
                    for (i++; i < size; i++) output << imageData[i];
                    output.close();
                    data.close();
                    return "file:" + iconPath;
                } while (idx < metaLength);
            }
            return "";
        }
        // note; this will load the file directly with sndfile
        Song *addFromPath(char *path, int playlist) {
            if (getSong(path)) return NULL;
            std::string iconUrl = extractArt(path);
            std::string name = path;
            int start = name.find_last_of('/') +1;
            int end = name.find_last_of('.');
            int length = end - start;
            name = name.substr(start, length);
            SF_INFO *info = new SF_INFO;
            SNDFILE *file = sf_open(path, SFM_READ, info);
            if (sf_error(file) > 0) {
                std::cout << "  Could not add " << name << "; " << sf_strerror(file) << "\n";
                return NULL;
            }
            std::cout << "Adding " << name << "\n";
            Song *song = new Song();
            song->path = path;
            song->artist = "Unknown";
            song->album = "";
            song->title = name;
            if (iconUrl.length() > 0) song->iconUrl = iconUrl;
            const char *artist = sf_get_string(file, SF_STR_ARTIST);
            const char *album = sf_get_string(file, SF_STR_ALBUM);
            const char *title = sf_get_string(file, SF_STR_TITLE);
            if (artist != NULL && artist[0] != '\0') song->artist = artist;
            if (album != NULL && album[0] != '\0') song->album = album;
            if (title != NULL && title[0] != '\0') song->title = title;
            addNewSong(song, playlist, false);
            sf_close(file);

            return song;
        }
        Song *createNewSong(std::string path, std::string artist, std::string album, std::string title, int playlist) {
            Song *song = new Song();
            song->path = path;
            song->artist = artist;
            song->album = album;
            song->title = title;
            addNewSong(song, playlist, true);

            return song;
        }
        Song *createNewSong(std::string path, std::string artist, std::string album, int playlist) {
            int start = path.find_last_of('/') +1;
            int end = path.find_last_of('.');
            int length = end - start;
            std::string name = path.substr(start, length);
            return createNewSong(path, artist, album, name, playlist);
        }
        Song *createNewSong(std::string path, std::string artist, int playlist) {
            int start = path.find_last_of('/') +1;
            int end = path.find_last_of('.');
            int length = end - start;
            std::string name = path.substr(start, length);
            return createNewSong(path, artist, "", name, playlist);
        }
        Song *createNewSong(std::string path, int playlist) {
            int start = path.find_last_of('/') +1;
            int end = path.find_last_of('.');
            int length = end - start;
            std::string name = path.substr(start, length);
            return createNewSong(path, "Unknown", "", name, playlist);
        }
        Song *createNewSong(int playlist) { return createNewSong("", "", "", "", playlist); }
        void addNewSong(Song *song, int playlist, bool unsafe = true) {
            if (unsafe && getSong(song->path) != NULL) return;
            Playlist *target = getPlaylist(playlist);
            playlists->songs.push_back(song);
            getArtist(song->artist)->songs.push_back(song);
            getArtist(song->album)->songs.push_back(song);
            if (playlists != target) target->songs.push_back(song);
        }
        void saveToDisk() {
            std::ofstream metaFile(getConfigPath());
            if (!metaFile.is_open()) {
                std::cerr << "No playlists file could be made/found; " << errno;
                return;
            }
            Playlist *list = playlists;
            // root wont be inserted by name, as inserting it by name would make it not root
            for (int i = 0; i < list->songs.size(); i++)
                metaFile << "; path: " + esc(list->songs[i]->path, true) <<
                    "; artist: " << esc(list->songs[i]->artist, true) <<
                    "; album: " << esc(list->songs[i]->album, true) <<
                    "; title: " << esc(list->songs[i]->title, true) <<
                    "; iconUrl: " << esc(list->songs[i]->iconUrl, true) << "\n";
            list = list->next;
            while (list != NULL) {
                metaFile << "\n/ " << esc(list->name, false) << "\n";
                for (int i = 0; i < list->songs.size(); i++) 
                    metaFile << "| " << esc(list->songs[i]->path, true) << "\n";
                list = list->next;
            }
            metaFile.flush();
            metaFile.close();
        }
};

PlaylistParser::PlaylistParser() {
    std::ifstream metaFile(getConfigPath());
    std::string line;
    Playlist *item = new Playlist();
    srandom(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    item->name = "Root";
    item->id = idIncr++;
    playlists = item;
    bool inName;
    while (getline(metaFile, line)) {
        if (line.length() <= 0) continue;
        switch (line[0]) {
        case ' ': continue;
        case '#': continue;
        case '/':
            item = item->next = new Playlist();
            item->id = idIncr++;
            item->name = "";
            inName = false;
            for (int i = 1; i < line.length(); i++) {
                if (!inName && line[i] == ' ') continue;
                inName = true;
                item->name += line[i];
            }
            item->songs.clear();
            break;
        case '|': {
            if (playlists == item) break;
            std::string path = "";
            inName = false;
            for (int i = 1; i < line.length(); i++) {
                if (!inName && line[i] == ' ') continue;
                inName = true;
                path += line[i];
            }
            Song *song = getSong(path);
            if (song == NULL) {
                std::cout << "fuck " << path;
                break;
            }
            item->songs.push_back(song);
            break;
        }
        case ';':
            Song *song = new Song();
            inName = false;
            std::string key = "";
            std::string value = "";
            int mode = 0;
            for (int i = 0; i < line.length(); i++) {
                switch (line[i]) {
                // reference by path
                case ';': inName = false; key = ""; mode = 0; continue;
                // path
                case ':': inName = false; value = ""; mode = 1; continue;
                // artist
                case '\\': i++;
                default:
                    if (!inName && line[i] == ' ') continue;
                    inName = true;
                    switch (mode) {
                    case 0: key += line[i]; break;
                    case 1: value += line[i]; break;
                    }
                    break;
                }
                if ((i == line.length() -1 || line[i +1] == ';')) {
                    if (key == "path") song->path = value;
                    else if (key == "artist") song->artist = value;
                    else if (key == "album") song->album = value;
                    else if (key == "title") song->title = value;
                    else if (key == "iconUrl") song->iconUrl = value;
                }
            }
            addNewSong(song, item->id, false);
            break;
        }
    }
}
#endif