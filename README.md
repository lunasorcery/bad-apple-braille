# bad-apple-braille

Bad Apple, but made with the unicode braille glyphs!

![](docs/screenshot.png)

## Building & Running

To generate the video datastream from source, run the following:

```
git clone --recurse-submodules https://github.com/lunasorcery/bad-apple-braille
cd bad-apple-braille/
make all
```

This depends on [youtube-dl](https://github.com/ytdl-org/youtube-dl) and [ffmpeg](https://ffmpeg.org/).

The resolution can be customized by modifying the values at the top of the Makefile.

If you just want to run the video, a ready-made 20x7 datastream is provided.  
This does not require the above dependencies and can be run as follows:

```
git clone https://github.com/lunasorcery/bad-apple-braille
cd bad-apple-braille/
make player
./player
```
