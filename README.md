# VDFS 7zip
This is a basic read-only implementation of Samsung's VDFS (Vertically Deliberate improved performance File System), used in their Tizen TVs as a 7zip plugin.   
It is based on the samsung released GPL source of VDFS. This plugin is also licensed under GPL.   
NOTE: the plugin will almost certainly only work on "CLEAN" images, as in, not ever written to, or read-only. Right now only tested on the 2007 layout (latest).   

# Installation (Windows)
- Built with `make` command  
- Locate your 7zip installation folder, for example `C:\Program Files\7-Zip`  
- Create a new directory called `Formats` inside that folder  
- Copy one of the built DLL files depending on your architecture into the new `Formats` folder.  
