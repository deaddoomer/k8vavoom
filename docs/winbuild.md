# K8VAVOOM Windows Build Guide

**ATTENTION** This guide is strictly unofficial and unsupported by K8Vavoom development team. Please, do not pester them with problems concerning building the software under Windows. The official way to build the windows binaries is to use MXE cross-compiler under 32-bit Linux system. 

## BUILD ENVIRONMENT
*Expert settings*
Software:
 - GCC compiler, version 6 to 9.
 - CMake
 - pkg-config

 Libraries:
 - OpenAL
 - SDL2
 - samplerate

*Regular settings*
There are two ways to set up the environment, MSYS and MinGW. The former automates mundane tasks on adding the required libraries, but it has the current version of GCC, so there could be some bug if with a newer version of the compiler changes its behavior. The latter requires more manual work, but it has no problem with the compiler version for now. 

All mentioned packages are for 64-bit systems. If you want to build k8vavoom as a 32-bit app, replace `x86_64` with `i686`.

### MSYS 
1. Visit msys2.org, download, install and update MSYS
2. Open MSYS 64-bit terminal and install the following packages using `pacman -S` command:
	- mingw-w64-x86_64-gcc
	- mingw-w64-x86_64-make
	- mingw-w64-x86_64-openal
	- mingw-w64-x86_64-SDL2
	- mingw-w64-x86_64-libsamplerate
	- mingw-w64-x86_64-cmake

### MinGW
This way is somewhat messy, so try to find another solution if you can.
1. Visit win-builds.org, download and install the system. It provides a full building environment, but the project looks  abandoned and the gcc within is too old to compile k8vavoom code.
2. Update gcc. For example, from https://nuwen.net/mingw.html#install. This package has the required gcc 9.2 and comes with nice cmd files for your building console as a bonus.
3. Visit packages.msys2.org, download packages for the following libraries:
	- openal
	- libsamplerate
4. Copy the folders from the packages into corresponding folders in your MinGW installation. Use [PeaZip](https://github.com/peazip/PeaZip/releases) to handle obscure archives like .xz or .zst

## BUILD
For you convenience,  it's recommended to create or use for the build a folder outside of the source tree. The build process in itself is very simple. First, you have to generate makefiles with CMake. The following command is recommended:
`cmake -S ./path/to/source -G "MinGW Makefiles" -DWITH_SSE4=ON`

If you want to make a 64-bit build, add WITH_WNT64 flag:
`cmake -S ./path/to/source -G "MinGW Makefiles" -DWITH_SSE4=ON -DWITH_WNT64=ON`

Due to some discrepancies in CMake configuration, without any CPU flags CMake would fallback on pentium4 processor definition. 

Then, you just run the mingw's make:
`mingw32-make`
*Note: it's called mingw32 even in 64-bit environment*

## INSTALL
The build process will generate the `k8vavoom.exe` file and `basev` directory. To make a runnable installation you have to do:
 1. Make a folder to host the software
 2. Make a subfolder named `share`
 3. Copy the generated directory `basev` into  `share`
 4. Find the folder `basev` in the k8vavoom *sources*
 5. Copy `games.txt` and `detectors.txt` into your `share\basev` folder
 6. Inside your `share` folder make a subfolder named `icons`
 7. Find the folder `res\ico` in the k8vavoom *sources*
 8. Copy `*.ico` files into your `share\icons` folder
 9. Copy `k8vavoom.exe` into your folder
 10.Copy  the following libraries into your folder:
	 - libgcc_s_seh-1.dll (x64)
	 - libgcc_s_dw2-1.dll (x32)
	 - libopenal-1.dll
	 - libstdc++-6.dll
	 - libwinpthread-1.dll
	 - SDL2.dll 
lib*.dll are located in your mingw(32|64) folder. You'll have to download SDL binaries separately.

Static linking options are currently under investigation.

That's all! Put your IWADs into `iwads` folder and happy DooMing!

## ACKNOWLEGEMENT 
All hail Ketmar Dark!

## AUTHORS
This document was written by:
	 - Stein Krauz (steinkrauz@yahoo.com)
	 
You may send authors emails with questions, bugreports or suggestions about building k8vavoom for Windows. The authors retain the right to ignore your mail or to delay their answers for as long as they seem fit.

## LICENSE 
This work is licensed under a [Creative Commons Attribution-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-sa/4.0/)



