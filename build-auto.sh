curl https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz -o ./libsndfile-1.2.2.tar.xz
rm -rf ./libsndfile-1.2.2.tar.xz
tar -xf ./libsndfile-1.2.2.tar.xz
cd ./libsndfile-1.2.2
mkdir -p '/data/data/com.termux/files/usr/lib'
bash ./libtool   --mode=install install -c   src/libsndfile.la '/data/data/com.termux/files/usr/lib'
install -c src/.libs/libsndfile.so.1.0.37 /data/data/com.termux/files/usr/lib/libsndfile.so.1.0.37
(cd /data/data/com.termux/files/usr/lib && { ln -s -f libsndfile.so.1.0.37 libsndfile.so.1 || { rm -f libsndfile.so.1 && ln -s libsndfile.so.1.0.37 libsndfile.so.1; }; })
(cd /data/data/com.termux/files/usr/lib && { ln -s -f libsndfile.so.1.0.37 libsndfile.so || { rm -f libsndfile.so && ln -s libsndfile.so.1.0.37 libsndfile.so; }; })
install -c src/.libs/libsndfile.lai /data/data/com.termux/files/usr/lib/libsndfile.la
PATH="/data/data/com.termux/files/usr/sbin:/data/data/com.termux/files/usr/bin:/data/data/com.termux/files/usr/sbin:/data/data/com.termux/files/usr/bin:/sbin:/bin:/snap/bin:/sbin" ldconfig -n /data/data/com.termux/files/usr/lib
mkdir -p '/data/data/com.termux/files/usr/bin'
bash ./libtool   --mode=install install -c programs/sndfile-info programs/sndfile-play programs/sndfile-convert programs/sndfile-cmp programs/sndfile-metadata-set programs/sndfile-metadata-get programs/sndfile-interleave programs/sndfile-deinterleave programs/sndfile-concat programs/sndfile-salvage '/data/data/com.termux/files/usr/bin'
install -c programs/.libs/sndfile-info /data/data/com.termux/files/usr/bin/sndfile-info
install -c programs/.libs/sndfile-play /data/data/com.termux/files/usr/bin/sndfile-play
install -c programs/.libs/sndfile-convert /data/data/com.termux/files/usr/bin/sndfile-convert
install -c programs/.libs/sndfile-cmp /data/data/com.termux/files/usr/bin/sndfile-cmp
install -c programs/.libs/sndfile-metadata-set /data/data/com.termux/files/usr/bin/sndfile-metadata-set
install -c programs/.libs/sndfile-metadata-get /data/data/com.termux/files/usr/bin/sndfile-metadata-get
install -c programs/.libs/sndfile-interleave /data/data/com.termux/files/usr/bin/sndfile-interleave
install -c programs/.libs/sndfile-deinterleave /data/data/com.termux/files/usr/bin/sndfile-deinterleave
install -c programs/.libs/sndfile-concat /data/data/com.termux/files/usr/bin/sndfile-concat
install -c programs/.libs/sndfile-salvage /data/data/com.termux/files/usr/bin/sndfile-salvage
mkdir -p '/data/data/com.termux/files/usr/share/doc/libsndfile'
install -c -m 644 docs/index.md docs/libsndfile.jpg docs/libsndfile.css docs/print.css docs/api.md docs/command.md docs/bugs.md docs/formats.md docs/sndfile_info.md docs/new_file_type_howto.md docs/win32.md docs/FAQ.md docs/lists.md docs/embedded_files.md docs/octave.md docs/tutorial.md '/data/data/com.termux/files/usr/share/doc/libsndfile'
mkdir -p '/data/data/com.termux/files/usr/include'
install -c -m 644 include/sndfile.h include/sndfile.hh '/data/data/com.termux/files/usr/include'
mkdir -p '/data/data/com.termux/files/usr/share/man/man1'
install -c -m 644 man/sndfile-info.1 man/sndfile-play.1 man/sndfile-convert.1 man/sndfile-cmp.1 man/sndfile-metadata-get.1 man/sndfile-metadata-set.1 man/sndfile-concat.1 man/sndfile-interleave.1 man/sndfile-deinterleave.1 man/sndfile-salvage.1 '/data/data/com.termux/files/usr/share/man/man1'
mkdir -p '/data/data/com.termux/files/usr/lib/pkgconfig'
install -c -m 644 sndfile.pc '/data/data/com.termux/files/usr/lib/pkgconfig'
pkg install libsndfile
g++ ./main.cpp -I/usr/local/include/freetype2 -lao -lsndfile -lncurses -lX11 -lXext -lfontconfig -lXft -Wwrite-strings -o ./dist/built -g
