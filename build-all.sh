g++ ./main.cpp -lao -lsndfile -lncurses -lnotify -lglib-2.0 -o ./dist/x86-64/play `pkg-config --cflags --libs glib-2.0` `pkg-config --cflags --libs gdk-pixbuf-2.0`
./build-local.sh;