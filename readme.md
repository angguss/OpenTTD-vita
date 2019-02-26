# OpenTTD port for PS Vita

## Extra Dependencies on top of base OpenTTD
 - A recent SDL2 Vita version with touch support. This comes pre-installed with current VitaSDK.

## Building
Configure with

```
PKG_CONFIG_PATH=$VITASDK/arm-vita-eabi/lib/pkgconfig ./configure --os=PSVITA --host arm-vita-eabi --enable-static --prefix=/usr/local/vitasdk --with-sdl="pkg-config sdl2" --without-fontconfig --disable-strip --enable-network=0 --without-liblzo2
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
 - The touch screen is currently indirect mouse input, like on a laptop touchpad style. Short tap for left click, single finger plus second finger short tap for right click. Drag with two fingers to drag and drop.
 - Use the D-Pad or right analog stick for panning the map.
 - Circle or right trigger for right-click
 - Cross or left trigger for left-click
 - Square zoom-in
 - Triangle zoom-out

### Current Limitations
 - No sound
 - No network support

### Other
An initial openttd.cfg can be found in [bin/openttd.cfg](https://github.com/angguss/OpenTTD-vita/blob/master/bin/openttd.cfg), the important parts performance-wise are
 - `resolution = 960,544`
 - `sprite_cache_size_px = 8`
