#ifndef __FL_BORDERLESSWINDOW_H__
#define __FL_BORDERLESSWINDOW_H__

////////////////////////////////////////////////////////////////////////////////
// Fl_BorderlessWindow , reversion 3.
// ---------------------------------

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_RGB_Image.H>

#ifdef USE_FL_BORDERLESSWINDOW_DRAGDROP_1
#include "Fl_TransBox.h"

#define DDROP_SERVER_TYPE   Fl_TransBox
#else
#define DDROP_SERVER_TYPE   Fl_Box
#endif // USE_FL_BORDERLESSWINDOW_DRAGDROP_1

////////////////////////////////////////////////////////////////////////////////

class Fl_BorderlessWindow;

class Fl_BorderlessWindowTitle : public Fl_Box
{
    public:
        Fl_BorderlessWindowTitle( int X, int Y, int W, int H, const char* T = NULL );

    public:
        void borderlesswindow( Fl_BorderlessWindow* w ) { bwin = w; }

    private:
        ~Fl_BorderlessWindowTitle();

    protected:
        int handle( int e );

    protected:
        bool     doubleclicked();
        unsigned getcurrenttimems();

    protected:
        Fl_BorderlessWindow* bwin;

    protected:
        bool        grabbed;
        int         grab_x;
        int         grab_y;
        int         dblclk_pre_x;
        int         dblclk_pre_y;
        int         dblclk_nxt_x;
        int         dblclk_nxt_y;
        unsigned    dblclk_pre_t;
        unsigned    dblclk_nxt_t;
};

class Fl_BorderlessDropServer : public DDROP_SERVER_TYPE
{
    public:
        Fl_BorderlessDropServer( int X, int Y, int W, int H,
                                 Fl_BorderlessWindow* parent = NULL )
        : DDROP_SERVER_TYPE( X, Y, W, H ), _parent(parent)
        {
#if USE_FL_BORDERLESSWINDOW_DRAGDROP_1
            align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
            labelcolor( 0xFFFFFF00 );
            label("Drag\nfolder\nhere");
            labelfont( FL_FREE_FONT );
            labelsize( 40 );
            color( 0 ); set_alpha( 0x7f );
            when( FL_WHEN_CHANGED );
#else
            align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
            labelcolor( 0xFFFFFF00 );
            label("Drag\nfolder\nhere");
            labelfont( FL_FREE_FONT );
            labelsize( 40 );
            color( 0 );
            when( FL_WHEN_CHANGED );
#endif // USE_FL_BORDERLESSWINDOW_DRAGDROP_1
        }

        ~Fl_BorderlessDropServer(){}

    protected:
        int handle( int e );

    protected:
        Fl_BorderlessWindow* _parent;
};

class Fl_BorderlessWindow : public Fl_Double_Window
{
    public:
        Fl_BorderlessWindow( int W, int H, const char* T = NULL );
        ~Fl_BorderlessWindow();

    public: /// overrides
        void begin();
        void end();
        void size_range(int minw, int minh, int maxw=0, int maxh=0);

    public:
        static void Fl_BorderlessWindow_CB( Fl_Widget* w, void* p );

    public:
        void setsinglewindow( int stype = 0 );
        void setcleartype( bool b );
        void titleicon( Fl_Image* i );
        void bgimage( Fl_Image* i, Fl_Align fitting = FL_ALIGN_LEFT_TOP );
        // Index of button images
        // 0: minimize
        // 1: maximize
        // 2: quit
        // 3: back to previous size.
        void controlbuttonsimage( int idx, Fl_Image* i );
        void refreshcontrolbuttonsimages();
#ifdef _WIN32
        void titleicon( HICON i );
#endif // _WIN32
        void color( Fl_Color c1, Fl_Color c2 );
        void color( Fl_Color bg );
        void labelcolor( Fl_Color c );
        void labelsize( Fl_Fontsize pix );
        void enlarge();
        void locksizing();
        void unlocksizing();
        void WCB( Fl_Widget* w );

    public:
        Fl_Image* bgimage()  \
                  { return backgroundimg; }
        Fl_Image* controlbuttonsimage( int idx ) \
                  { if ( idx > 3 ) return NULL; \
                    return (Fl_Image*)imgWindowButtons[ idx ]; }

    public:
        void      clientarea_sizes( int &x, int &y, int &w, int &h );
        int       clientarea_x();
        int       clientarea_y();
        int       clientarea_w();
        int       clientarea_h();
        Fl_Group* clientarea();

    public:
        void procdblclktitle();
        void procwindowmove( int x, int y )
            { if ( maximized_wh == false ) position( x, y ); }
        void callback_onmove( Fl_Callback* cb, void* p )
            { cbOnMove = cb; pdOnMove = p; }
        void callback_onsized( Fl_Callback* cb, void* p )
            { cbOnSized = cb; pdOnSized = p; }
        void callback_ondropfiles( Fl_Callback* cb, void* p )
            { cbOnDrop = cb; pdOnDrop = p; }
        const char* dragdropfiles()
            { return ddropfiles; }
        int  dohandle( int e )
            { return handle(e); }
        int  shown()
            { return realshown; }
        bool maximized()
            { return maximized_wh; }

    protected:
        int  sizegriptest( int x, int y );
        int  handle( int e );
        void draw();
        void releaseconvtitleiconimg();
        void regenbgcache( int w, int h );
        void reorders();

    protected:
        bool    sizegripped;
        int     sizegrip_flag;
        int     sizegrip_m;
        int     sizegrip_x;
        int     sizegrip_y;
        int     sizegrip_w;
        int     sizegrip_h;
        bool    maximized_wh;
        int     prev_fs_x;
        int     prev_fs_y;
        int     prev_fs_w;
        int     prev_fs_h;
        int     singlewindowtype;
        int     min_w;
        int     min_h;
        int     max_w;
        int     max_h;
        int     realshown;
        bool    sizelocked;
        bool    dragdropenabled;
        bool    dragdroping;

    private:
        Fl_Boxtype                  defaultboxtype;
        Fl_Box*                     boxWindowIcon;
        Fl_BorderlessWindowTitle*   boxWindowTitle;
        Fl_Button*                  boxWindowButtons[3];
        Fl_Image*                   imgWindowButtons[4];
        Fl_Group*                   grpInnerWindow;
        Fl_RGB_Image*               convtitleiconimg;
        Fl_Image*                   backgroundimg;
        Fl_RGB_Image*               backgroundimgcached;
        Fl_Align                    backgroundfitting;
        Fl_Callback*                cbOnMove;
        void*                       pdOnMove;
        Fl_Callback*                cbOnSized;
        void*                       pdOnSized;
        Fl_Callback*                cbOnDrop;
        void*                       pdOnDrop;
        char*                       ddropfiles;
        Fl_BorderlessDropServer*    ddropServer;
};


#endif /// of __FL_BORDERLESSWINDOW_H__
