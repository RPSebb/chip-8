$env:PATH="C:/mingw-w64/bin;$env:PATH"
gcc -std=c2y -O2 src/window_win32.c src/main.c src/audio.c -o app.exe -lgdi32 -lole32
