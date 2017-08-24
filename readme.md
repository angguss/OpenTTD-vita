# OpenTTD port for PS Vita

## Extra Dependencies on top of base OpenTTD
 - [SDL Vita](https://github.com/angguss/SDL-Vita) with touch support. SDL 1.2 is not required for vita build
 - [debugnet](https://github.com/psxdev/debugnet)

## Building
Configure with

```
PKG_CONFIG_PATH=$VITASDK/arm-vita-eabi/lib/pkgconfig ./configure --os=PSVITA --host arm-vita-eabi --enable-static --prefix=/usr/local/vitasdk --with-sdl="pkg-config sdl2" --without-fontconfig --disable-strip --enable-network=0
```

Then build

```
make && cd cmake && make && cd ..
```

openttd.vpk can be found in __cmake/__

## Running
OpenTTD will attempt to load everything from ux0:/data/openttd/

The easiest way to populate this is to install the PC version and copy the files across. In the future when network support is re-enabled the content downloader should be functional and this step will be skippable.

### Controls
 - The touch screen is currently mouse input, press and hold for dragging.
 - Use the D-Pad for panning.
 - Circle for right-click (hold while tapping)
 - Triggers for zoom (right trigger zoom-in, left trigger zoom-out)

### Current Limitations
 - The game is set to run at 480x272 scaled to the screen size for the sake of speed. It will run at 960x544 or 720x408 but don't expect more than ~10-15 FPS
 - No sound
 - No network support

### Other
An initial openttd.cfg can be found in [bin/openttd.cfg](https://github.com/angguss/OpenTTD-vita/blob/master/bin/openttd.cfg), the important parts performance-wise are
 - `resolution = 480,272`
 - `sprite_cache_size_px = 8`
