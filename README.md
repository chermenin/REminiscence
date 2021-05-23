
# REminiscence / REinforced

> Release version: 0.5.2  
> Release date: 2021-05-23


## About

REminiscence is a re-implementation of the engine used in the game Flashback
made by Delphine Software and released in 1992. More informations about the
game can be found at [1], [2] and [3].

REinforced is an enhanced version of the original REminiscence from http://cyxdown.free.fr/reminiscence/ and based on the version `0.4.6`.


## Data Files

You will need the original files of the PC (DOS or CD), Amiga or Macintosh
release. Support for Amiga and Macintosh is still experimental.

For the Macintosh release, the resource fork must be dumped as a file named
`FLASHBACK.BIN` (MacBinary) or `FLASHBACK.RSRC` (AppleDouble).

To hear music during polygonal cutscenes with the PC version, you need to copy
the `music` directory of the Amiga version or use the `.mod` fileset from
unexotica [4].

For speech with in-game dialogues, you need to copy the `VOICE.VCE` file from
the SegaCD version to the DATA directory.


## Running

By default, the engine tries to load the game data files from the `DATA`
directory, as the original game executable did. The savestates are saved in the
`SAVE` directory. All sounds and music will be loaded from the `TUNES` directory.

These paths can be changed using command line switches :

    Usage: fb [OPTIONS]...
        --datapath=PATH   Path to data files (default 'DATA')
        --savepath=PATH   Path to save files (default 'SAVE')
        --tunepath=PATH   Path to sound and music files (default 'TUNES')
        --levelnum=NUM    Start to level, bypass introduction
        --window          Play in window
        --widescreen=MODE 16:9 display (adjacent,mirror,blur,none)
        --scaler=NAME@X   Graphics scaler (default 'scale@3')
        --language=LANG   Language (fr,en,de,sp,it,jp,ru)
        --autosave        Save game state automatically

The scaler option specifies the algorithm used to smoothen the image in
addition to a scaling factor. External scalers are also supported, the suffix
shall be used as the name. Eg. If you have `scaler_tv2x.dll`, you can pass
`--scaler tv2x` to use that algorithm with a doubled window size (512x448).

The widescreen option accepts the following modes:

- `adjacent` - left and right rooms bitmaps will be drawn
- `adjacent-blur` - left and right rooms bitmaps will be blurred
- `mirror` - the current room bitmap will be drawn mirrored
- `blur` - the current room bitmap will be blurred

In-game hotkeys :

    Arrow Keys      move Conrad
    Enter           use the current inventory object
    Shift           talk / use / run / shoot
    Escape          display the options
    Backspace       display the inventory
    Alt Enter       toggle windowed/fullscreen mode
    Alt + and -     increase or decrease game screen scaler factor
    Alt S           write screenshot as .tga
    Ctrl S          save game state
    Ctrl L          load game state
    Ctrl R          rewind game state buffer (requires --autosave)
    Ctrl + and -    change game state slot
    Function Keys   change game screen scaler


## Credits

Delphine Software, obviously, for making another great game.  
Yaz0r, Pixel and gawd for sharing information they gathered on the game.


## Contacts

Original REminiscence: Gregory Montoir, cyx@users.sourceforge.net  
REinforced version: Alex Chermenin, alex@chermenin.ru


## URLs:

[1] http://www.mobygames.com/game/flashback-the-quest-for-identity  
[2] http://en.wikipedia.org/wiki/Flashback:_The_Quest_for_Identity  
[3] http://ramal.free.fr/fb_en.htm  
[4] https://www.exotica.org.uk/wiki/Flashback  
