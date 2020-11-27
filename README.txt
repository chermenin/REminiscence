
REminiscence README
Release version: 0.4.6
-------------------------------------------------------------------------------


About:
------

REminiscence is a re-implementation of the engine used in the game Flashback
made by Delphine Software and released in 1992. More informations about the
game can be found at [1], [2] and [3].


Data Files:
-----------

You will need the original files of the PC (DOS or CD), Amiga or Macintosh
release. Support for Amiga and Macintosh is still experimental.

For the Macintosh release, the resource fork must be dumped as a file named
'FLASHBACK.BIN' (MacBinary) or 'FLASHBACK.RSRC' (AppleDouble).

To hear music during polygonal cutscenes with the PC version, you need to copy
the music/ directory of the Amiga version or use the .mod fileset from
unexotica [4].

For speech with in-game dialogues, you need to copy the 'VOICE.VCE' file from
the SegaCD version to the DATA directory.


Running:
--------

By default, the engine tries to load the game data files from the 'DATA'
directory, as the original game executable did. The savestates are saved in the
current directory.

These paths can be changed using command line switches :

    Usage: rs [OPTIONS]...
    --datapath=PATH   Path to data files (default 'DATA')
    --savepath=PATH   Path to save files (default '.')
    --levelnum=NUM    Level to start from (default '0')
    --fullscreen      Fullscreen display
    --widescreen=MODE 16:9 display (adjacent,mirror,blur,none)
    --scaler=NAME@X   Graphics scaler (default 'scale@3')
    --language=LANG   Language (fr,en,de,sp,it,jp)
    --autosave        Save game state automatically

The scaler option specifies the algorithm used to smoothen the image in
addition to a scaling factor. External scalers are also supported, the suffix
shall be used as the name. Eg. If you have scaler_xbr.dll, you can pass
'--scaler xbr@2' to use that algorithm with a doubled window size (512x448).

The widescreen option accepts two modes :
    'adjacent' : left and right rooms bitmaps will be drawn
    'mirror' : the current room bitmap will be drawn mirrored

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

Debug hotkeys :

    Ctrl F          toggle fast mode
    Ctrl I          Conrad 'infinite' life
    Ctrl B          toggle display of updated dirty blocks


Credits:
--------

Delphine Software, obviously, for making another great game.
Yaz0r, Pixel and gawd for sharing information they gathered on the game.


Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net


URLs:
-----

[1] http://www.mobygames.com/game/flashback-the-quest-for-identity
[2] http://en.wikipedia.org/wiki/Flashback:_The_Quest_for_Identity
[3] http://ramal.free.fr/fb_en.htm
[4] https://www.exotica.org.uk/wiki/Flashback
