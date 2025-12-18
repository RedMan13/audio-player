g++ ./main.cpp -lao -lsndfile -lncurses -o ./dist/x86-64/play `pkg-config --cflags --libs dbus-c++-1` `pkg-config --cflags --libs glib-2.0`
./build-local.sh;