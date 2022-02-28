20210930

MyHook - this app needs a better name. M.Brent.

This app is for Windows 2000 or NT4.0 with dozens of service packs only - 
it uses low level hooks for that. It will run on Win9x using regular hooks,
which do not always work as reliably. Some features may not work on Win9x.

This program was originally used to find the mouse cursor with a keypress, 
now it does a lot more. Right-clicking to system tray icon gives a menu 
that lets you alter the type of the mouse find animation, or basic tasks:

Spiral	- the mouse animation circles around the mouse point and spirals down\
Surround- eight big pointers converge from all sides (tends to be slow)\
Blink	- a big point blinks and points to the mouse. May be hidden if at the bottom of the screen\
Explode - the mouse pointer blows up (always transparent)\
Random	- a random effect is chosen each time\
Debug	- when this is on, every keypress pops up a message box, and tells you the scancode of the key, and the command string if one is attached. Turn debug back off as soon as you're done with it. If you hit hundreds of keys and crash the system, well, d'uh! If no keys are defined it will also tell you so.\
About	- tells you who's responsible\
Quit	- unload the hook. You can also double-click the icon with the left button.

All other functions are defined in the .INI file. The INI, Myhook.ini, is first 
looked for in the current directory (which is in Documents and Settings\\\<name\>\\
Local Settings on Win2000), or the Windows directory if not found there. The INI 
is re-read and then updated and saved on exit, so it's safe to change it while 
the program is running, but you will have to close and re-launch to load the 
changes.

Note that the INI is erased and re-written when the program exits. It will 
preserve one comment per key command if the comment is immediately after the 
Key line.

Commands for defining keys are entered as so:\
Key \<scancode in hex\> \<command\> [\<arguments\>...]

The word Key may be upper or lowercase but must be followed by a space.\
The scancode must be entered in hexadecimal, and ranges from 01 to 1ff (with keys above ff being extended).\
The command ranges from 1 to 6, and is followed by the desired arguments.

Commands:\
1 - Find Mouse - no Arguments\
2 - Send WM_COMMAND message to a Window. Format: 2 <cmd#> <class name>\
    The cmd# is the command number of the button or menu action that you want to activate, in decimal. Hooks32 will help in finding these codes.\
    The class name is the Window Class name to find. Duplicates of the same program may not all get the message. Hooks32 will show class names.\
21- This is a special case coded especially for Winamp, to make a Play/Pause button work correctly. No arguments.\
3 - Set Master volume - Format: 3 <value>\
    The value is either 0 to toggle mute, or a percentage step from 1 to 100, or -1 to -100 (in decimal).\
4 - Copy a string to the text clipboard: Format: 3 <String>\
    This copies the specified string into the Clipboard so you can paste it into an application.\
5 - Enable/Disable Window under Mouse - Format: 5 <flag>\
    This sends an 'enable window' message to the Window under the pointer (normally you would use this on a button - it won't work on menus). If flag is 1 it is enabled, if flag is 0 it is disabled.\
6 - Run application - Format: 6 <action> <start string>\
    This calls ShellExecute to perform the desired action on the specified file. Actions are any string that's valid on the document type, as seen in the right-click popup menu. Normal values are "open", "edit", etc. The start string should be a full path to the document or exe. Default actions aren't supported, you have to tell the system which one you want.\
7 - Type string (Windows 2000 and above only) - Format: 7 <string>\
	This function enters the specified string into the current window as if you had typed the keys on the keyboard. It does not affect the clipboard. It is	ignored if the OS is not Windows 2000 or above.\
8 - Set Window Translucency (Windows 200 and above) - Format: 8 <0-255>\
	This function sets the window that the mouse is pointing at to translucent,	with an alpha from 0 (transparent) to 255 (solid). Note that the mode is permanently changed (till you close the window) and updates, or especially	dragging the window, will be much slower.

My Sample INI:

```
; Config File for RunHook program
; Automatically generated on exit
; Comments must start with semicolon and be immediately after
; the key in question (maximum of 40 characters)
;
; To define keys (the only thing you should change)
; Enter a line line this:
; KEY <HEX SCANCODE> <COMMAND> [<Arguments as needed>]
; Values are entered in Decimal except for the ScanCode.
; Commands are:
; 1 - Find Mouse (original purpose of this program! ;)
; 2 - Send Command to Window with WM_COMMAND: <cmd#> <class name>
; 3 - Set Volume: <value> - value is 0 to mute, or + or - 1-100 for % step
; 4 - Copy string to clipboard: <string>
; 5 - Enable/Disable Window under Mouse: <1 to enable, 0 to disable>
; 6 - Run application: <action (edit, open, etc)> <application run string>
;
DisplayMode: 4
DrawMode: 1

Key 46 1
; Scroll lock - Find Mouse

Key 58 4 (555)555-1212
; F12 - Copy to Clipboard (A phone number)

Key 110 2 40044 Winamp v1.x
; Prev key for Winamp

Key 119 2 40048 Winamp v1.x
; Next key for Winamp

Key 11f 5 1
; Rocket key - Enable Window

Key 121 6 open calc
; Magnifying Glass to open calculator

Key 122 21
; Play/Pause key for Winamp

Key 123 3 0
; "i" key for Mute toggle 

Key 124 2 40047 Winamp v1.x
; Stop button for Winamp
```

What's new?

13 July 2001 - Added menu options for 'Edit INI' and 'Reload INI' to simplify that task\
			 - Added 'type string' function for Windows 2000

30 Sep 2021  - graphical fixes to make it work better in Windows 11
