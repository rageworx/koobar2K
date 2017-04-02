#ifndef __WMAIN_H__
#define __WMAIN_H__

#include <FL/Fl.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Image.H>

#include "Fl_BorderlessWindow.H"
#include "Fl_GroupAniSwitch.h"

#include <list>
#include <vector>
#include <string>

#include <pthread.h>

#include "mpg123wrap.h"
#include "audioout.h"

class wMain
{
    public:
        wMain( int argc, char** argv );

    public:
        int     Run();
        void*   PThreadCall();
        void    WidgetCB( Fl_Widget* w );
        void    MenuCB( Fl_Widget* w );
        void    MoveCB( Fl_Widget* w );

    private:
        ~wMain();

    protected:
        void parseParams();
        void createComponents();
        void applyThemes();
        void loadTags();
        void loadArtCover();
        unsigned image_color_illum( Fl_RGB_Image* img );

    protected:
        int     _argc;
        char**  _argv;

    protected:
        bool        _runsatfullscreen;
        bool        _keyprocessing;
        bool        _threadkillswitch;
        pthread_t   _pt;

    protected:
        Fl_BorderlessWindow*    mainWindow;
        Fl_Group*               grpViewer;
        Fl_Box*                 boxCoverArt;
        Fl_Box*                 boxArtist;
        Fl_Box*                 boxAlbum;
        Fl_Box*                 boxTitle;
        Fl_Box*                 boxMiscInfo;
        Fl_Group*               grpStatus;
        Fl_Group*               grpOverlay;
        Fl_Box*                 boxStatus;
        Fl_GroupAniSwitch*      testswitch;
        Fl_RGB_Image*           winbgimg;

    protected:
        AudioOut*               aout;
        MPG123Wrap*             mp3dec;
        std::string             strtag_artist;
        std::string             strtag_title;
        std::string             strtag_album;
        std::string             strtag_moreinfo;
};

#endif /// of __WINMAIN_H__
