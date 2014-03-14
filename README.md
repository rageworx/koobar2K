koobar2K
========

A Project of multi-platform simple audio player with MPG123 and FLTK.
The name, "koobar2K" is - as you catched - parody of foobar2000.
Actually foobar2000 works on only Windows, but I want use it on my LinuxMint without WINE.
So I have plan to make it my own audio player, and it named to "koobar2K".

I have plan to use libmpg123 to play my mp3 audio files.
As I know default mpg123 console based player (is this a front-end ?) is not so functional, no GUI.
I have experience in customized libmpg123 with ALSA driver in Cortex-A8 cpu with Wolfson 8 series codec, So I'm sure my koobar2K project will successfully runs on my LinuxMint and Windows system with FLTK.

Will defect: FLTK drawing refresh is not so good. Especially drawing multiple channel (FFT) graphics will be limited by its drawing rate and halt in processing something in FLTK's internal. 
