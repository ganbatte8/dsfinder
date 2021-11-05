This is a tool made to find, list and verify doublesteps in Stepmania .sm files.
I have used an early prototype of this program to find a mistake in one of the early releases of Saitama's Ultimate Weapon in about one morning.
The program listed over 800 doublesteps for that pack, only one of which appeared to be a mistake to me, and that mistake was confirmed.
Manually verifying each candidate through ArrowVortex or the Stepmania editor would have been unreasonably tedious; 
the custom GUI is there to reduce the time spent switching files and scrolling in an editor to basically zero.
Hope this is valuable to you guys.

## Getting started
Look at the Releases section on the right of this github web page. 
Click on "first release". 
Download the .zip file for your OS, extract it. 
Run the executable (either win32_dsfinder.exe or linux_dsfinder).
A file explorer should open with the current path initialized to that of the calling process.
On Windows you can press F11 to toggle fullscreen.
Navigate in the file explorer using the arrow keys, left and right to exit or enter directories.
The current directory path is drawn in the yellow rectangle above.
Whenever you press spacebar, it will look for .sm files (or .SM .Sm .sM) in that directory tree.
When it's done, dsfinder mode opens and shows a list of all the doublesteps. 
Navigate in that list with up and down arrows.
When a doublestep is selected, the surrounding notes are drawn. 
Hold left click and drag with the mouse to navigate up/down in the notes stream.
Left click on the option checkboxes on the right to toggle them.
Press spacebar again to go back to file explorer mode.
Alternatively you can navigate through everything using an X360 gamepad: 
Dpad and left joystick for folder navigation, right joystick for scrolling up or down the notes stream, 
A to toggle between dsfinder/file explorer modes, B/X/Y to toggle the options.



