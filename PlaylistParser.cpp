#include <sndfile.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

struct Song {
    std::string path;
    std::string artist;
    std::string album;
    std::string title;
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
                    case '#':
                    case '|':
                    case '-':
                    case '?':
                    case '/':
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
        // note; this will load the file directly with sndfile
        void addFromPath(char *path, int playlist) {
            if (getSong(path)) return;
            SF_INFO *info;
            SNDFILE *file = sf_open(path, SFM_READ, info);
            std::string name = path;
            name = name.substr(name.find_last_of('/') +1, name.find_last_of('.'));
            if (sf_error(file) > 0) {
                std::cout << "  Could not add " << name << "; " << sf_strerror(file) << "\n";
                return;
            }
            std::cout << "Adding " << name << "\n";
            Song *song = new Song();
            song->path = path;
            song->artist = "Unknown";
            song->album = "None";
            song->title = name;
            const char *artist = sf_get_string(file, SF_STR_ARTIST);
            const char *album = sf_get_string(file, SF_STR_ALBUM);
            const char *title = sf_get_string(file, SF_STR_TITLE);
            if (artist != NULL) song->artist = artist;
            if (album != NULL) song->album = album;
            if (title != NULL) song->title = title;
            Playlist *target = getPlaylist(playlist);
            playlists->songs.push_back(song);
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
                metaFile << "- " + esc(list->songs[i]->path, true) <<
                    "? " << esc(list->songs[i]->artist, true) <<
                    "/ " << esc(list->songs[i]->album, true) <<
                    ": " << esc(list->songs[i]->title, true) << "\n";
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
        default:
            Song *song = new Song();
            inName = false;
            std::string collector;
            int mode = 0;
            for (int i = 0; i < line.length(); i++) {
                switch (line[i]) {
                // reference by path
                case '|': inName = false; mode = 4; continue;
                // path
                case '-': inName = false; mode = 0; continue;
                // artist
                case '?': inName = false; mode = 1; continue;
                // album
                case '/': inName = false; mode = 2; continue;
                // title
                case ':': inName = false; mode = 3; continue;
                case '\\': i++;
                default:
                    if (!inName && line[i] == ' ') continue;
                    inName = true;
                    switch (mode) {
                    case 0: song->path += line[i]; break;
                    case 1: song->artist += line[i]; break;
                    case 2: song->album += line[i]; break;
                    case 3: song->title += line[i]; break;
                    case 4:
                        collector += line[i];
                        if (i == (line.length() -1)) {
                            song = getSong(collector);
                            collector = "";
                        }
                        break;
                    }
                    break;
                }
            }
            item->songs.push_back(song);
            // always push to root!!
            if (item != playlists) playlists->songs.push_back(song);
            getArtist(song->artist)->songs.push_back(song);
            getArtist(song->album)->songs.push_back(song);
            break;
        }
    }
}