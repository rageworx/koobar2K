#ifndef __WMAIN_H__
#define __WMAIN_H__

#include <FL/Fl.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Button.H>

#include "Fl_BorderlessWindow.H"
//#include "Fl_GroupAniSwitch.h"
#include "Fl_SeekBar.h"
#include "Fl_TransBox.h"
#include "Fl_FocusEffectButton.H"
#include "Fl_NobackScroll.H"

#include <list>
#include <vector>
#include <string>

#include <pthread.h>

#include "mpg123wrap.h"
#include "audioout.h"

class wMain : public AudioOutEvent
{
    public:
        wMain( int argc, char** argv );

    public:
        int   Run();
        void* PThreadCall();
        void  WidgetCB( Fl_Widget* w );
        void  MenuCB( Fl_Widget* w );
        void  MoveCB( Fl_Widget* w );
        void  SizedCB( Fl_Widget* w );
        void  DDropCB( Fl_Widget* w );
        void  ListCB( Fl_Widget* w );
        void  DelayedWorkCB();

    private:
        ~wMain();

    protected:
        void OnNewBuffer( unsigned position, unsigned nbsz, unsigned maxposition );
        void OnBufferEnd();

    protected:
        void parseParams();
        void createComponents();
        void applyThemes();
        void loadTags();
        void loadArtCover();
        void updateTrack();
        void updateInfo();
        void updateOverlayBG();
        void setNoArtCover();
        void switchPlayButton( int state = 0 );
        void makePlayList( const char* refdir = NULL );
        void playControl( int action );
        unsigned image_color_illum( Fl_RGB_Image* img );
        void requestWindowRedraw();

    protected:
        int     _argc;
        char**  _argv;

    protected:
        bool        _runsatfullscreen;
        bool        _keyprocessing;
        bool        _threadkillswitch;
        bool        _firstthreadrun;
        bool        _mp3art_loaded;
        bool        _mp3onplaying;
        int         _mp3controlnext;
        pthread_t   _pt;

    protected:
        Fl_BorderlessWindow*    mainWindow;
        Fl_Group*               grpViewer;
        Fl_Box*                 boxCoverArt;
        Fl_SeekBar*             skbProgress;
        Fl_Box*                 boxTrackNo;
        Fl_Box*                 boxArtist;
        Fl_Box*                 boxAlbum;
        Fl_Box*                 boxTitle;
        Fl_Box*                 boxMiscInfo;
        Fl_Box*                 boxFileInfo;
        Fl_Group*               grpControl;
        Fl_TransBox*            boxControlBG;
        Fl_FocusEffectButton*   btnPrevTrk;
        Fl_FocusEffectButton*   btnPlayStop;
        Fl_FocusEffectButton*   btnNextTrk;
        Fl_FocusEffectButton*   btnRollUp;
        Fl_FocusEffectButton*   btnVolCtrl;
        Fl_FocusEffectButton*   btnListCtrl;
        Fl_Group*               grpOverlay;
        //Fl_GroupAniSwitch*      testswitch;
        Fl_Box*                 boxOverlayBG;
        Fl_NobackScroll*        sclMp3List;
        // ----
        Fl_RGB_Image*           imgPlayCtrls[4];
        Fl_RGB_Image*           imgWinBG;
        Fl_RGB_Image*           imgNoArt;
        Fl_RGB_Image*           imgOverlayBg;
        Fl_Color                colLabelNoArt;
        Fl_Color                colLabelArt;

    protected:
        AudioOut*               aout;
        MPG123Wrap*             mp3dec;
        unsigned                mp3_channels;
        unsigned                mp3_frequency;
        unsigned                mp3dpos_cur;
        unsigned                mp3dpos_max;
        unsigned                window_min_h;
        std::string             strinf_trackno;
        std::string             strtag_artist;
        std::string             strtag_title;
        std::string             strtag_album;
        std::string             strtag_moreinfo;
        std::string             strtag_fileinfo;

    protected:
        std::vector< std::string >  mp3list;
        std::vector< unsigned >     mp3listLUT;
        unsigned                    mp3queue;
};

#endif /// of __WINMAIN_H__
