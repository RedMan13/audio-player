g++ ./main.cpp -I/usr/include/gdk-pixbuf-2.0 -lao -lsndfile -lncurses -lX11 -lXext -lfontconfig -lXft -lnotify -lglib-2.0 -o ./dist/x86-64/play `pkg-config --cflags --libs glib-2.0`
./build-local.sh;