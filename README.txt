Dynamic Next Stream - OBS Dock Plugin v1.0.4
============================================

Dynamic Next Stream is a native OBS dock for planning upcoming streams and
displaying a live countdown plus schedule preview.


Package Contents
----------------

  dynamic-next-stream-1.0.4\
    INSTALL.bat
    README.txt
    obs-plugins\
      64bit\
        dynamic-next-stream.dll
        dynamic-next-stream.pdb
    data\
      obs-plugins\
        dynamic-next-stream\
          locale\
            en-US.ini
            de-DE.ini


Installation
------------

Recommended: double-click `INSTALL.bat`, choose your OBS folder, and let the
script copy the files for you.

Manual installation: copy BOTH top-level folders, `obs-plugins\` and `data\`,
into your OBS Studio installation directory.


Usage
-----

Open the dock from:

  View -> Docks -> Next Stream

Inside the dock you can:

  - define a weekly stream schedule
  - configure formatting, categories, and day labels
  - preview the rendered output live
  - export the result to a text file if needed
  - write a Discord-ready weekly text file


Requirements
------------

  - OBS Studio 30.x / 31.x / 32.x (Windows x64)
  - Qt 6 (provided by OBS)


License
-------

GPLv2 or later.
