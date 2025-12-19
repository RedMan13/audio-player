g++ ./main.cpp -lao -lsndfile -lncurses -lsdbus-c++ -o ./dist/x86-64/play `pkg-config --cflags --libs glib-2.0`
./build-local.sh;