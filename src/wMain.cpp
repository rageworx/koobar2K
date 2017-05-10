#ifdef _WIN32
    #include <windows.h>
#endif // of _WIN32

#include <unistd.h>
#include <dirent.h>

#include <cstdio>
#include <cstring>

#include <algorithm>

#include "wMain.H"
#include <FL/fl_ask.H>
#include <FL/Fl_draw.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_JPEG_Image.H>

#include "fl_imgtk.h"
#include "audiodxsnd.h"
#include "dirsearch.h"

#include "themes.h"
#include "resource.h"

////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace fl_imgtk;

////////////////////////////////////////////////////////////////////////////////

#define DEF_APP_NAME            "FLTK MPG123 GUI"
#define DEF_APP_DEFAULT_W       400
#define DEF_APP_DEFAULT_H       600

#ifdef DEBUG
#define UI_LOCK();              if ( ui_locks>=0 ) { Fl::lock();ui_locks++; printf("ui_locks=%d\n",ui_locks); }
#define UI_UNLOCK();            if ( ui_locks>0 ) { Fl::unlock();ui_locks--; printf("ui_locks=%d\n",ui_locks); }
#else
#define UI_LOCK()               if ( ui_locks>=0 ) { Fl::lock();ui_locks++; }
#define UI_UNLOCK()             if ( ui_locks>0 ) { Fl::unlock();ui_locks--; }
#endif // DEBUG

////////////////////////////////////////////////////////////////////////////////

void fl_w_cb( Fl_Widget* w, void* p );
void fl_list_cb( Fl_Widget* w, void* p );
void fl_move_cb( Fl_Widget* w, void* p );
void fl_sized_cb( Fl_Widget* w, void* p );
void fl_ddrop_cb( Fl_Widget* w, void* p );
void fl_delayed_work_timer_cb( void* p );

void* pthread_play( void* p );

////////////////////////////////////////////////////////////////////////////////

vector<string> split_string( const string& str, const string& delimiter )
{
    vector<string> strings;

    string::size_type pos = 0;
    string::size_type prev = 0;

    while ( (pos = str.find(delimiter, prev)) != string::npos )
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }

    strings.push_back(str.substr(prev));

    return strings;
}

////////////////////////////////////////////////////////////////////////////////

wMain::wMain( int argc, char** argv )
 : _argc( argc ),
   _argv( argv ),
   _runsatfullscreen( false ),
   _keyprocessing( false ),
   _firstthreadrun( true ),
   _pt( NULL ),
   //testswitch( NULL ),
   _mp3art_loaded( false),
   _mp3onplaying( false ),
   _mp3controlnext( -1 ),
   ui_locks( 0 ),
   imgWinBG( NULL ),
   imgNoArt( NULL ),
   imgOverlayBg( NULL ),
   mp3list( NULL ),
   aout( NULL )
{
    parseParams();
    createComponents();

    colLabelNoArt = 0x33333300;
    colLabelArt   = 0xCFCFCF00;

#ifdef _WIN32
    HICON
    hIconWindowLarge = (HICON)LoadImage( fl_display,
                                         MAKEINTRESOURCE( IDC_ICON_A ),
                                         IMAGE_ICON,
                                         64,
                                         64,
                                         LR_SHARED );

    HICON
    hIconWindowSmall = (HICON)LoadImage( fl_display,
                                         MAKEINTRESOURCE( IDC_ICON_A ),
                                         IMAGE_ICON,
                                         16,
                                         16,
                                         LR_SHARED );

    SendMessage( fl_xid( mainWindow ),
                 WM_SETICON,
                 ICON_BIG,
                 (LPARAM)hIconWindowLarge );

    SendMessage( fl_xid( mainWindow ),
                 WM_SETICON,
                 ICON_SMALL,
                 (LPARAM)hIconWindowSmall );

    mainWindow->titleicon( hIconWindowSmall );

#endif // _WIN32

    if ( _runsatfullscreen == true )
    {
        mainWindow->enlarge();
    }

    // Init audio.
    HWND hParent = fl_xid( mainWindow );

    // default set to 44KHz stereo.
    mp3_frequency = 44100;
    mp3_channels  = 2;

    aout = new AudioDXSound( hParent, this );

    if ( aout != NULL )
    {
        aout->InitAudio( mp3_channels, mp3_frequency );
    }

    mp3list = new Mp3List();
}

wMain::~wMain()
{
    mpg123wrap_exit();

    liststrs.clear();

    if ( mp3list != NULL )
        delete mp3list;

    if ( aout != NULL )
    {
        aout->Control( AudioOut::STOP, 0, 0 );
        aout->FinalAudio();

        delete aout;
    }

    for( unsigned cnt=0; cnt<4; cnt++ )
    {
        fl_imgtk::discard_user_rgb_image( imgPlayCtrls[cnt] );
    }

    fl_imgtk::discard_user_rgb_image( imgWinBG );
    fl_imgtk::discard_user_rgb_image( imgNoArt );
    fl_imgtk::discard_user_rgb_image( imgOverlayBg );
}

int wMain::Run()
{
    _threadkillswitch = false;

#ifdef DEBUG
    makePlayList( "./test" );
#endif // DEBUG

    if ( _argc > 1 )
    {
        for( unsigned cnt=1; cnt<_argc; cnt++ )
        {
            if ( DirSearch::DirTest( _argv[cnt] ) == true )
            {
                makePlayList( _argv[cnt] );
            }
        }
    }

    mainWindow->deactivate();
    Fl::add_timeout( 0.5f, fl_delayed_work_timer_cb, this );

    return Fl::run();
}

void* wMain::PThreadCall()
{
    _mp3onplaying = false;

    if ( aout == NULL )
        return NULL;

    mp3dec = new MPG123Wrap();

    if ( mp3dec == NULL )
        return NULL;

    if ( mp3list->Size() == 0 )
        return NULL;

    mp3dpos_cur = 0;
    mp3dpos_max = 0;

    unsigned idx = mp3list->PlayQueue( mp3queue );
    const char* mp3fn = mp3list->FileName( idx );

    if ( mp3fn != NULL )
    {
        if ( mp3dec->Open( mp3fn ) == false )
        {
            delete mp3dec;
            mp3dec = NULL;
            return NULL;
        }
    }
    else
    {
        delete mp3dec;
        mp3dec = NULL;
        return NULL;
    }

    switchPlayButton( 0 );

    // Display progress to first ...
    OnNewBuffer( 1, 10, 1000000 );

    loadTags();
    loadArtCover();
    updateOverlayBG();

    Fl::awake( mainWindow );
    mainWindow->redraw();

    unsigned failure_count = 0;

    while( _threadkillswitch == false )
    {
        unsigned char* abuff = NULL;

        if ( mp3dec != NULL )
        {
            bool isnext = false;

            if ( mp3dpos_max > 0 )
            {
                mp3dec->DecodeFramePos( mp3dpos_cur, mp3dpos_max );
            }

            unsigned abuffsz = mp3dec->DecodeFrame( abuff, &isnext );

            Mp3Item *pItem = mp3list->Get( mp3queue );

            if ( ( mp3_frequency != pItem->frequency() ) ||
                 ( mp3_channels  != pItem->channels() ) )
            {
                mp3_frequency = pItem->frequency();
                mp3_channels  = pItem->channels();

                aout->InitAudio( mp3_channels, mp3_frequency );
            }

            if ( abuffsz > 0 )
            {
                if ( aout->WriteBuffer( abuff, abuffsz ) == AudioOut::OK )
                {

                    if ( _mp3onplaying == false )
                    {
                        aout->Control( AudioOut::PLAY, 0, 0 );

                        if ( aout->ControlState() == AudioOut::PLAY )
                        {
                            switchPlayButton( 1 );
                            _mp3onplaying = true;
                        }
                    }
                }
                else
                {
                    mp3dec->Close();
                    delete mp3dec;
                    mp3dec = NULL;

                    return NULL;
                }
            }
            else
            if ( isnext == false )
            {
                mp3dec->Close();
                delete mp3dec;
                mp3dec = NULL;

                if ( _mp3onplaying == false )
                {
                    playControl( 1 );
                }
            }
        }
        else
        {
            // Give 10ms wait to thread for thread noop.
            Sleep( 10 );
        }
    }

    return NULL;
}

void wMain::parseParams()
{
}

void wMain::createComponents()
{
    int ver_i[] = {APP_VERSION};
    static char wintitle[128] = {0};

    sprintf( wintitle, "%s, version %d.%d.%d.%d",
             DEF_APP_NAME,
             ver_i[0],
             ver_i[1],
             ver_i[2],
             ver_i[3] );

    mainWindow = new Fl_BorderlessWindow( DEF_APP_DEFAULT_W,DEF_APP_DEFAULT_H, wintitle );
    if ( mainWindow != NULL)
    {
        int cli_x = mainWindow->clientarea_x();
        int cli_y = mainWindow->clientarea_y();
        int cli_w = mainWindow->clientarea_w();
        int cli_h = cli_y + mainWindow->clientarea_h();

        mainWindow->setcleartype( true );

        extern unsigned char pngimg_minimize[];
        extern unsigned pngimg_minimize_size;

        extern unsigned char pngimg_maximize[];
        extern unsigned pngimg_maximize_size;

        extern unsigned char pngimg_quit[];
        extern unsigned pngimg_quit_size;

        extern unsigned char pngimg_b2size[];
        extern unsigned pngimg_b2size_size;

        Fl_RGB_Image* imgwb[4] = {NULL};

        imgwb[0] = (Fl_RGB_Image*)new Fl_PNG_Image( "winbtn1",
                                                    pngimg_minimize,
                                                    pngimg_minimize_size );

        imgwb[1] = (Fl_RGB_Image*)new Fl_PNG_Image( "winbtn2",
                                                    pngimg_maximize,
                                                    pngimg_maximize_size );

        imgwb[2] = (Fl_RGB_Image*)new Fl_PNG_Image( "winbtn3",
                                                    pngimg_quit,
                                                    pngimg_quit_size );

        imgwb[3] = (Fl_RGB_Image*)new Fl_PNG_Image( "winbtn4",
                                                    pngimg_b2size,
                                                    pngimg_b2size_size );

        for( unsigned cnt=0; cnt<4; cnt++ )
        {
            fl_imgtk::invert_ex( imgwb[cnt] );
            mainWindow->controlbuttonsimage( cnt, imgwb[cnt] );
            delete imgwb[cnt];
        }

        grpViewer = new Fl_Group( cli_x, cli_y , cli_w, cli_h );
        if ( grpViewer != NULL )
        {
            grpViewer->begin();
        }

        // --------------------------

        int test_w = cli_w - 20;
        int test_h = test_w;

        boxCoverArt = new Fl_Box( cli_x + 10 , cli_y, test_w, test_h );
        if ( boxCoverArt != NULL )
        {
            boxCoverArt->box( FL_NO_BOX );
        }

        cli_y += test_h;

        skbProgress = new Fl_SeekBar( cli_x + 10 , cli_y, test_w, 3 );

        cli_y += 3;

        boxTitle = new Fl_MarqueeLabel( cli_x + 10 , cli_y, test_w, 25 );
        if ( boxTitle != NULL )
        {
            boxTitle->label( "No title" );
            boxTitle->labelcolor( 0x33333300 );
            boxTitle->labelfont( FL_FREE_FONT );
            boxTitle->labelsize( 20 );
            //boxTitle->drawself( false );
            boxTitle->checkcondition();
        }

        cli_y += 25;

        boxAlbum = new Fl_Box( cli_x + 10 , cli_y, test_w, 18 );
        if ( boxAlbum != NULL )
        {
            boxAlbum->label( "No album" );
            boxAlbum->labelcolor( 0x33333300 );
            boxAlbum->labelfont( FL_FREE_FONT );
            boxAlbum->labelsize( 14 );
        }

        cli_y += 18;

        boxArtist = new Fl_Box( cli_x + 10 , cli_y, test_w, 18 );
        if ( boxArtist != NULL )
        {
            boxArtist->label( "No artist" );
            boxArtist->labelcolor( 0x33333300 );
            boxArtist->labelfont( FL_FREE_FONT );
            boxArtist->labelsize( 14 );
        }

        cli_y += 18;

        boxMiscInfo = new Fl_Box( cli_x + 10 , cli_y, test_w, 15 );
        if ( boxMiscInfo != NULL )
        {
            boxMiscInfo->label( "No more information" );
            boxMiscInfo->labelcolor( 0x33333300 );
            boxMiscInfo->labelfont( FL_FREE_FONT );
            boxMiscInfo->labelsize( 12 );
        }

        cli_y += 15;

        boxFileInfo = new Fl_Box( cli_x + 10 , cli_y, test_w, 13 );
        if ( boxFileInfo != NULL )
        {
            boxFileInfo->label( "No file information" );
            boxFileInfo->labelcolor( 0x33333300 );
            boxFileInfo->labelfont( FL_FREE_FONT );
            boxFileInfo->labelsize( 11 );
        }

        cli_y += 13;

        boxTrackNo = new Fl_Box(  cli_x , cli_y, cli_w, 10, "000/000" );
        if ( boxTrackNo != NULL )
        {
            boxTrackNo->align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
            boxTrackNo->labelfont( FL_FREE_FONT );
            boxTrackNo->labelcolor( 0x33333300 );
            boxTrackNo->labelsize( 10 );
        }

        cli_y += 10;

        Fl_Box* boxsizer = new Fl_Box( cli_x, cli_y, cli_w, 1 );
        if ( boxsizer != NULL )
        {
            boxsizer->box( FL_NO_BOX );
            grpViewer->resizable( boxsizer );
        }

        cli_y += 1;

        // Here control panel.
        // Let make 5 pixel left for Fl_BorderlessWindow can size grip works.
        cli_y = cli_h - 80 - 5;

        grpControl = new Fl_Group( cli_x , cli_y, cli_w, 80 );
        if ( grpControl != NULL )
        {
            grpControl->begin();

            boxControlBG = new Fl_TransBox( grpControl->x(),
                                            grpControl->y(),
                                            grpControl->w(),
                                            grpControl->h() );
            if ( boxControlBG != NULL )
            {
                boxControlBG->color( 0 ); /// Black !
                boxControlBG->set_alpha( 0x7F );
            }

            // Play Controls ...
            // [ 1 ]  [ 2 ]  [ 3 ]
            // 48x48, 64x64, 48x48

            extern unsigned pngimg_ctrlplay_size;
            extern unsigned char pngimg_ctrlplay[];
            extern unsigned pngimg_ctrlstop_size;
            extern unsigned char pngimg_ctrlstop[];
            extern unsigned pngimg_ctrlprev_size;
            extern unsigned char pngimg_ctrlprev[];
            extern unsigned pngimg_ctrlnext_size;
            extern unsigned char pngimg_ctrlnext[];

            imgPlayCtrls[0] = new Fl_PNG_Image( "ctrlPlay",
                                                pngimg_ctrlplay,
                                                pngimg_ctrlplay_size );
            imgPlayCtrls[1] = new Fl_PNG_Image( "ctrlStop",
                                                pngimg_ctrlstop,
                                                pngimg_ctrlstop_size );
            imgPlayCtrls[2] = new Fl_PNG_Image( "ctrlPrev",
                                                pngimg_ctrlprev,
                                                pngimg_ctrlprev_size );
            imgPlayCtrls[3] = new Fl_PNG_Image( "ctrlNext",
                                                pngimg_ctrlnext,
                                                pngimg_ctrlnext_size );

            cli_y += ( grpControl->h() - 64 ) / 2;

            int ctrlCenter = mainWindow->w() / 2;

            int ctrlx = ctrlCenter - 32;

            btnPlayStop = new Fl_FocusEffectButton( ctrlx, cli_y, 64, 64, "@>" );
            if ( btnPlayStop != NULL )
            {
                btnPlayStop->drawparent( mainWindow->clientarea() );
                btnPlayStop->box( FL_NO_BOX );
                btnPlayStop->color( FL_BLACK, FL_BLACK );
                btnPlayStop->align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
                btnPlayStop->clear_visible_focus();

                if ( imgPlayCtrls[0] != NULL )
                {
                    btnPlayStop->image( imgPlayCtrls[0] );
                    btnPlayStop->label( NULL );
                }

                btnPlayStop->callback( fl_w_cb, this );
            }

            int ctrly = cli_y + ( ( 64 - 48 ) / 2);

            ctrlx = ctrlCenter - 32 - 64;

            btnPrevTrk = new Fl_FocusEffectButton( ctrlx, ctrly, 48, 48, "@<<" );
            if ( btnPrevTrk != NULL )
            {
                btnPrevTrk->drawparent( mainWindow->clientarea() );
                btnPrevTrk->box( FL_NO_BOX );
                btnPrevTrk->color( FL_BLACK, FL_BLACK );
                btnPrevTrk->align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
                btnPrevTrk->clear_visible_focus();

                if ( imgPlayCtrls[2] != NULL )
                {
                    btnPrevTrk->image( imgPlayCtrls[2] );
                    btnPrevTrk->label( NULL );
                }

                btnPrevTrk->callback( fl_w_cb, this );
            }

            ctrlx = ctrlCenter + 32 + 16;

            btnNextTrk = new Fl_FocusEffectButton( ctrlx, ctrly, 48, 48, "@>>");
            if ( btnNextTrk != NULL )
            {
                btnNextTrk->drawparent( mainWindow->clientarea() );
                btnNextTrk->box( FL_NO_BOX );
                btnNextTrk->color( FL_BLACK, FL_BLACK );
                btnNextTrk->align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );
                btnNextTrk->clear_visible_focus();

                if ( imgPlayCtrls[3] != NULL )
                {
                    btnNextTrk->image( imgPlayCtrls[3] );
                    btnNextTrk->label( NULL );
                }

                btnNextTrk->callback( fl_w_cb, this );
            }

            ctrlx = 10;
            ctrly = grpControl->y() + 10;

            btnRollUp = new Fl_FocusEffectButton( ctrlx, ctrly, 20, 20, "^" );
            if ( btnRollUp != NULL )
            {
                btnRollUp->box( FL_NO_BOX );
                btnRollUp->clear_visible_focus();

                extern unsigned pngimg_ctrl_rollup_size;
                extern unsigned char pngimg_ctrl_rollup[];

                Fl_Image* pimg = new Fl_PNG_Image( "ctrlRollup",
                                                    pngimg_ctrl_rollup,
                                                    pngimg_ctrl_rollup_size );

                if ( pimg != NULL )
                {
                    btnRollUp->image( (Fl_RGB_Image*)pimg );
                    btnRollUp->drawparent( mainWindow->clientarea() );
                    btnRollUp->label( NULL );

                    delete pimg;
                }

                btnRollUp->callback( fl_w_cb, this );
            }

            ctrly = grpControl->y() + grpControl->h() - 30;

            btnVolCtrl = new Fl_FocusEffectButton( ctrlx, ctrly, 20, 20, "V" );
            if ( btnVolCtrl != NULL )
            {
                btnVolCtrl->box( FL_NO_BOX );
                btnVolCtrl->clear_visible_focus();

                extern unsigned pngimg_ctrl_spkr_size;
                extern unsigned char pngimg_ctrl_spkr[];

                Fl_Image* pimg = new Fl_PNG_Image( "ctrlVolume",
                                                    pngimg_ctrl_spkr,
                                                    pngimg_ctrl_spkr_size );

                if ( pimg != NULL )
                {
                    btnVolCtrl->image( (Fl_RGB_Image*)pimg );
                    btnVolCtrl->drawparent( mainWindow->clientarea() );
                    btnVolCtrl->label( NULL );

                    delete pimg;
                }

                btnVolCtrl->callback( fl_w_cb, this );
            }

            ctrlx = grpControl->w() - 30;

            btnListCtrl = new Fl_FocusEffectButton( ctrlx, ctrly, 20, 20, "L");
            if ( btnListCtrl != NULL )
            {
                btnListCtrl->box( FL_NO_BOX );
                btnListCtrl->clear_visible_focus();

                extern unsigned pngimg_ctrl_list_size;
                extern unsigned char pngimg_ctrl_list[];

                Fl_Image* pimg = new Fl_PNG_Image( "ctrlList",
                                                    pngimg_ctrl_list,
                                                    pngimg_ctrl_list_size );

                if ( pimg != NULL )
                {
                    btnListCtrl->image( (Fl_RGB_Image*)pimg );
                    btnListCtrl->drawparent( mainWindow->clientarea() );
                    btnListCtrl->label( NULL );

                    delete pimg;
                }

                btnListCtrl->callback( fl_w_cb, this );
            }

            grpControl->end();
        }

        // --------------------------

        if ( grpViewer != NULL )
        {
            grpViewer->end();
            mainWindow->clientarea()->resizable( grpViewer );
        }

        mainWindow->clientarea_sizes( cli_x, cli_y, cli_w, cli_h );

        grpOverlay = new Fl_Group( cli_x, cli_y, cli_w, cli_h );
        if ( grpOverlay != NULL )
        {
            grpOverlay->box( FL_NO_BOX );
            grpOverlay->align( FL_ALIGN_CLIP );
            grpOverlay->begin();

            boxOverlayBG = new Fl_Box( grpOverlay->x(), grpOverlay->y(),
                                       grpOverlay->w(), grpOverlay->h() );
            if ( boxOverlayBG != NULL )
            {
                boxOverlayBG->label( NULL );
                boxOverlayBG->box( FL_NO_BOX );
                boxOverlayBG->align( FL_ALIGN_CLIP );
            }

            sclMp3List = new Fl_NobackScroll( grpOverlay->x(), grpOverlay->y(),
                                              grpOverlay->w(), grpOverlay->h());
            if ( sclMp3List != NULL )
            {
                sclMp3List->color( FL_BLACK );
                sclMp3List->end();
            }

            grpOverlay->end();
            grpOverlay->hide();
        }

        mainWindow->end();
        mainWindow->callback( fl_w_cb, this );
        mainWindow->callback_onmove( fl_move_cb, this );
        mainWindow->callback_onsized( fl_sized_cb, this );
        mainWindow->callback_ondropfiles( fl_ddrop_cb, this );

        window_min_h = mainWindow->clientarea_y()
                       + boxCoverArt->h()
                       + skbProgress->h()
                       + 2;

        mainWindow->size_range( DEF_APP_DEFAULT_W, window_min_h, DEF_APP_DEFAULT_W );

        mainWindow->sitckymaximize( true );
        mainWindow->showscreencenter();
        mainWindow->wait_for_expose();
    }

    setNoArtCover();
    applyThemes();
}

void wMain::loadTags()
{
    if ( mp3dec == NULL )
        return;

    uchar* tmpstr = NULL;
    unsigned tmpstrsz = 0;

    strtag_artist.clear();
    strtag_title.clear();
    strtag_album.clear();
    strtag_moreinfo.clear();

    int realidx = mp3list->PlayQueue( mp3queue );

    Mp3Item* pItem = mp3list->Get( realidx );

    char tmpmap[80] = {0};

    if ( pItem != NULL )
    {
        if ( pItem->track() != NULL )
        {
            strtag_title = pItem->track();
            strtag_title += ". ";
        }

        if ( pItem->title() != NULL )
        {
            strtag_title += pItem->title();
        }

        if ( pItem->album() != NULL )
        {
            strtag_album = pItem->album();
        }

        if ( pItem->artist() != NULL )
        {
            strtag_artist = pItem->artist();
        }

        if ( pItem->year() != NULL )
        {
            strtag_artist += " (";
            strtag_artist += pItem->year();
            strtag_artist += ")";
        }

        if ( pItem->genre() != NULL )
        {
            strtag_moreinfo = pItem->genre();
        }

        UI_LOCK();

        mainWindow->lock();

        boxTitle->label( strtag_title.c_str() );
        boxAlbum->label( strtag_album.c_str() );
        boxArtist->label( strtag_artist.c_str() );
        boxMiscInfo->label( strtag_moreinfo.c_str() );

        updateInfo();

        mainWindow->unlock();
        UI_UNLOCK();

        boxTitle->checkcondition();
    }
}

void wMain::updateTrack()
{
    char tmpmap[80] = {0};

    strinf_trackno.clear();

    if ( mp3list->Size() > 0 )
    {
        sprintf( tmpmap, "%03d/%03d", mp3queue + 1, mp3list->Size() );
        strinf_trackno = tmpmap;
    }
    else
    {
        strinf_trackno = "000/000";
    }

    UI_LOCK();
    boxTrackNo->label( strinf_trackno.c_str() );
    UI_UNLOCK();
}

void wMain::updateInfo()
{
    updateTrack();

    char tmpmap[80] = {0};

    strtag_fileinfo.clear();

    Mp3Item* pItem = mp3list->Get( mp3queue );

    if ( pItem == NULL )
        return;

   // file info ---
    if ( pItem->mode() != NULL )
    {
        strtag_fileinfo = pItem->mode();
    }

    if ( pItem->frequency() > 0 )
    {
        sprintf( tmpmap,
                 " - %.1fKHz",
                 (float)pItem->frequency() / (float)1000 );

        strtag_fileinfo += tmpmap;
    }

    if ( pItem->bitratetype() != NULL )
    {
        strtag_fileinfo += " - ";
        strtag_fileinfo += pItem->bitratetype();
    }

    if ( pItem->bitrate() != NULL )
    {
        sprintf( tmpmap,
                 " - %dKbps",
                 pItem->bitrate() );

        strtag_fileinfo += tmpmap;
    }

    if ( pItem->layer() != NULL )
    {
        sprintf( tmpmap,
                 " - mpeg.l%d",
                 pItem->layer() );

        strtag_fileinfo += tmpmap;
    }

    UI_LOCK();
    boxFileInfo->label( strtag_fileinfo.c_str() );
    UI_UNLOCK();
}

void wMain::updateOverlayBG()
{
    static bool uobgmutexd = false;

    if ( uobgmutexd == true )
        return;

    uobgmutexd = true;

    /*
    Fl_RGB_Image* previmg = imgOverlayBg;
    imgOverlayBg = NULL;

    mainWindow->damage( 1 );
    mainWindow->redraw();
    grpViewer->damage( 1 );
    grpViewer->redraw();

    Fl_RGB_Image* tmpimg = fl_imgtk::drawblurred_widgetimage( mainWindow,
                                                              mainWindow->w() / 50 );

    UI_LOCK();
    mainWindow->lock();

    boxOverlayBG->deimage();
    sclMp3List->bgimg( NULL );

    if ( tmpimg != NULL )
    {
        imgOverlayBg = fl_imgtk::crop( tmpimg,
                                       mainWindow->clientarea_x(),
                                       mainWindow->clientarea_y(),
                                       mainWindow->clientarea_w(),
                                       mainWindow->clientarea_h() );

        fl_imgtk::discard_user_rgb_image( tmpimg );
    }


    if( grpOverlay->visible_r() > 0 )
    {
        grpOverlay->redraw();
    }

    mainWindow->unlock();
    fl_imgtk::discard_user_rgb_image( previmg );

    UI_UNLOCK();

    */

    uobgmutexd = false;
}

void wMain::setNoArtCover()
{
    if ( imgNoArt == NULL )
    {
        extern unsigned pngimg_noart_size;
        extern unsigned char pngimg_noart[];

        Fl_PNG_Image* imgtmp = new Fl_PNG_Image( "noart", pngimg_noart, pngimg_noart_size );
        if ( imgtmp != NULL )
        {
            imgNoArt = fl_imgtk::rescale( (Fl_RGB_Image*)imgtmp,
                                          boxCoverArt->w(),
                                          boxCoverArt->h(),
                                          fl_imgtk::BILINEAR );

            delete imgtmp;
        }
    }

    UI_LOCK();

    mainWindow->lock();
    mainWindow->bgimage( NULL );

    fl_imgtk::discard_user_rgb_image( imgWinBG );

    imgWinBG = fl_imgtk::makegradation_h( DEF_APP_DEFAULT_W, DEF_APP_DEFAULT_H,
                                          0xCCCCCC00, 0x99999900, true );

    mainWindow->color( 0xCCCCCC00 );
    mainWindow->bgimage( imgWinBG, FL_ALIGN_CENTER );

    mainWindow->labelcolor( colLabelNoArt );
    boxArtist->labelcolor( colLabelNoArt );
    boxAlbum->labelcolor( colLabelNoArt );
    boxTitle->labelcolor( colLabelNoArt );
    boxMiscInfo->labelcolor( colLabelNoArt );
    boxFileInfo->labelcolor( colLabelNoArt );
    btnRollUp->labelcolor( colLabelNoArt );
    btnVolCtrl->labelcolor( colLabelNoArt );
    btnListCtrl->labelcolor( colLabelNoArt );

    boxTrackNo->labelcolor( fl_darker( colLabelNoArt ) );

    if ( _mp3art_loaded == true )
    {
        for( unsigned cnt=0; cnt<4; cnt++ )
        {
            Fl_Image* pimg = mainWindow->controlbuttonsimage( cnt );
            if ( pimg != NULL )
            {
                fl_imgtk::invert_ex( (Fl_RGB_Image*)pimg );
            }
        }
    }

    Fl_RGB_Image* beremimg = (Fl_RGB_Image*) boxCoverArt->deimage();
    boxCoverArt->image( imgNoArt );

    if ( beremimg != NULL )
    {
        fl_imgtk::discard_user_rgb_image( beremimg );
    }

    mainWindow->unlock();
    UI_UNLOCK();

    _mp3art_loaded = false;
}

void wMain::switchPlayButton( int state )
{
    if ( state > 1 )
        return;

    if ( btnPlayStop != NULL )
    {
        UI_LOCK();

        if ( imgPlayCtrls[state] != NULL )
        {
            btnPlayStop->image( imgPlayCtrls[state] );
        }
        else
        {
            switch( state )
            {
                case 0 : btnPlayStop->label( "@>" ); break;
                case 1 : btnPlayStop->label( "@square" ); break;
            }
        }

        btnPlayStop->redraw();

        UI_UNLOCK();
    }

    mainWindow->redraw();
}

void wMain::makePlayList( const char* refdir )
{
    fl_cursor( FL_CURSOR_WAIT );
    if ( mp3list->AddListDir( refdir ) > 0 )
    {
        mp3list->ShufflePlayIndex();
    }

    updateTrack();

    fl_cursor( FL_CURSOR_DEFAULT );

    mp3queue = 0;
}

void wMain::playControl( int action )
{
    if ( action > 3 )
        return;

    //UI_LOCK();

    switch( action )
    {
        case 0 : /// PLAY
            {
                if ( mp3list->Size() > 0 )
                {
                    if ( ( _mp3onplaying == false ) && ( _pt == NULL ) )
                    {
                        _threadkillswitch = false;
                        _mp3onplaying = false;
                        int ret = pthread_create( &_pt, NULL, pthread_play, this );
                    }
                }
            }
            break;

        case 1: /// STOP.
            {
                if ( mp3list->Size() > 0 )
                {
                    if ( ( _mp3onplaying == true ) && ( _pt != NULL ) )
                    {
                        // Wait for thread ends.
                        if ( _threadkillswitch == false )
                        {
                            _threadkillswitch = true;
                            pthread_join( _pt, NULL );
                            _pt = NULL;

                            _threadkillswitch = false;
                            if ( mp3dec != NULL )
                            {
                                mp3dec->Close();
                            }

                            if ( aout != NULL )
                            {
                                aout->Control( AudioOut::STOP, 0, 0 );
                            }

                            _mp3onplaying = false;
                        }
                    }
                    else
                    {
                        if ( _pt != NULL )
                        {
                            _threadkillswitch = true;
                            pthread_join( _pt, NULL );
                            _pt = NULL;

                            _threadkillswitch = false;

                            if ( aout != NULL )
                            {
                                aout->Control( AudioOut::STOP, 0, 0 );
                            }
                        }
                        else
                        {
                            if ( aout != NULL )
                            {
                                aout->Control( AudioOut::STOP, 0, 0 );
                            }
                        }
                    }
                }
            }
            break;

        case 2: /// Previous Track
            {
                if ( mp3list->Size() > 0 )
                {
                    bool playedcntrl = false;

                    if ( _mp3onplaying == true )
                    {
                        playedcntrl = true;
                        playControl( 1 );
                    }

                    Fl::wait( 10 );

                    if ( mp3queue == 0 )
                    {
                        mp3queue = mp3list->Size() - 1;
                    }
                    else
                    {
                        mp3queue--;
                    }

                    Fl::wait( 10 );

                    if ( ( playedcntrl == true ) || ( _mp3controlnext == true ) )
                    {
                        playControl( 0 );
                    }
                }
            }
            break;

        case 3: /// Next Track
            {
                if ( mp3list->Size() > 0 )
                {
                    bool playedcntrl = false;

                    if ( _mp3onplaying == true )
                    {
                        playedcntrl = true;
                        playControl( 1 );
                    }

                    mp3queue++;

                    if( mp3queue >= mp3list->Size() )
                    {
                        mp3queue = 0;
                    }

                    if ( ( playedcntrl == true ) || ( _mp3controlnext == true ) )
                    {
                        playControl( 0 );
                    }
                }
            }
            break;
    }

    //UI_UNLOCK();
}

void wMain::loadArtCover()
{
    if ( mp3dec == NULL )
        return;

    int idx = mp3list->PlayQueue( mp3queue );
    Mp3Item* pItem = mp3list->Get( idx );

    if ( pItem == NULL )
        return;

    const char* img = pItem->coverart();
    string imgmime  = pItem->coverartmime();
    unsigned imgsz  = pItem->coverartsize();

    if ( ( img == NULL ) || ( imgsz == 0 ) )
    {
        setNoArtCover();
        return;
    }

    extern unsigned artmasksz;
    extern unsigned char artmask[];

    Fl_PNG_Image* amaskimg = new Fl_PNG_Image( "artmask", artmask, artmasksz );
    Fl_Image* coverimg = NULL;

    if ( ( imgmime == "image/jpeg" ) || ( imgmime == "image/jpg" ) )
    {
        coverimg = (Fl_Image*)new Fl_JPEG_Image( "coverart_jpg", (uchar*)img );
    }
    else
    if ( imgmime == "image/png" )
    {
        coverimg = (Fl_Image*)new Fl_PNG_Image( "coverart_png", (uchar*)img, imgsz );
    }

    if ( coverimg == NULL )
    {
        setNoArtCover();
        return;
    }

    int test_w = boxCoverArt->w();
    int test_h = boxCoverArt->h();

    if ( ( amaskimg != NULL ) && ( coverimg != NULL ) )
    {
        Fl_RGB_Image* rsdmask = fl_imgtk::rescale( (Fl_RGB_Image*)amaskimg,
                                                   test_w, test_h,
                                                   fl_imgtk::BILINEAR );

        Fl_RGB_Image* rsdart = fl_imgtk::rescale( (Fl_RGB_Image*)coverimg,
                                                   test_w, test_h,
                                                   fl_imgtk::BILINEAR );

        // Change background image with cover art.
        mainWindow->lock();
        mainWindow->bgimage( NULL );
        mainWindow->unlock();

        fl_imgtk::discard_user_rgb_image( imgWinBG );

        // Get Maximum size
        unsigned maxwh = max( mainWindow->w(), mainWindow->h() );

        imgWinBG = fl_imgtk::rescale( (Fl_RGB_Image*)coverimg,
                                      maxwh, maxwh,
                                      fl_imgtk::BILINEAR );

        if ( imgWinBG != NULL )
        {
            fl_imgtk::blurredimage_ex( imgWinBG, 10 );
            fl_imgtk::brightbess_ex( imgWinBG, -50.f );

            UI_LOCK();
            mainWindow->lock();
            mainWindow->color( FL_BLACK );
            mainWindow->bgimage( imgWinBG, FL_ALIGN_CENTER );

            // No need get illumination, it always draws bright color.
            mainWindow->labelcolor( colLabelArt );
            boxArtist->labelcolor( colLabelArt );
            boxAlbum->labelcolor( colLabelArt );
            boxTitle->labelcolor( colLabelArt );
            boxMiscInfo->labelcolor( colLabelArt );
            boxFileInfo->labelcolor( colLabelArt );
            btnRollUp->labelcolor( colLabelArt );
            btnVolCtrl->labelcolor( colLabelArt );
            btnListCtrl->labelcolor( colLabelArt );
            boxTrackNo->labelcolor( fl_darker( colLabelArt ) );

            mainWindow->unlock();
            UI_UNLOCK();

            if ( _mp3art_loaded == false )
            {
                for( unsigned cnt=0; cnt<4; cnt++ )
                {
                    Fl_Image* pimg = mainWindow->controlbuttonsimage( cnt );
                    if ( pimg != NULL )
                    {
                        fl_imgtk::invert_ex( (Fl_RGB_Image*)pimg );
                    }
                }
            }

            UI_LOCK();
            mainWindow->lock();
            mainWindow->refreshcontrolbuttonsimages();
            mainWindow->unlock();
            UI_UNLOCK();

            _mp3art_loaded = true;
        }

        delete amaskimg;
        delete coverimg;

        uchar* amask = NULL;
        unsigned amasksz = fl_imgtk::makealphamap( amask, rsdmask );

        if ( amasksz > 0 )
        {
            Fl_RGB_Image* img = fl_imgtk::applyalpha(  rsdart, amask, amasksz );

            delete[] amask;

            UI_LOCK();
            Fl_RGB_Image* remimg = (Fl_RGB_Image*) boxCoverArt->deimage();
            boxCoverArt->image( img );
            boxCoverArt->redraw();
            UI_UNLOCK();

            if ( remimg != NULL )
            {
                fl_imgtk::discard_user_rgb_image( remimg );
            }
        }

        fl_imgtk::discard_user_rgb_image( rsdmask );
        fl_imgtk::discard_user_rgb_image( rsdart );
    }
    else
    {
        if ( amaskimg != NULL )
        {
            delete amaskimg;
        }

        if ( coverimg != NULL )
        {
            delete coverimg;
        }

        setNoArtCover();
    }
}

unsigned wMain::image_color_illum( Fl_RGB_Image* img )
{
    if ( img != NULL )
    {
        unsigned imgsz = img->w() * img->h();
        unsigned sum = 0;

        uchar* imgptr = (uchar*)img->data()[0];

        for( unsigned cnt=0; cnt<imgsz; cnt++ )
        {
            uchar* ptr = &imgptr[ cnt * img->d() ];
            unsigned pixum = 0;

            for( unsigned rpt=0; rpt<img->d(); rpt++ )
            {
                pixum += ptr[ rpt ];
            }

            pixum /= img->d();
            sum += pixum;
            sum /= 2;
        }

        return sum;
    }

    return 0;
}

void wMain::applyThemes()
{
    rkrawv::InitTheme();

    if ( mainWindow != NULL )
    {
        rkrawv::ApplyBWindowTheme( mainWindow );
    }
}

void wMain::WidgetCB( Fl_Widget* w )
{
    if ( w == btnPlayStop )
    {
        btnPlayStop->deactivate();

        if ( mp3list->Size() > 0 )
        {
            if ( _mp3onplaying == false )
            {
                playControl( 0 );
                _mp3controlnext = 1;
            }
            else
            {
                _mp3controlnext = -1;
                playControl( 1 );
                switchPlayButton( 0 );
            }
        }

        btnPlayStop->activate();

        return;
    }

    if ( w == btnPrevTrk )
    {
        btnPrevTrk->deactivate();

        if ( mp3list->Size() > 0 )
        {
            playControl( 2 );
        }

        btnPrevTrk->activate();
        return;
    }

    if ( w == btnNextTrk )
    {
        btnNextTrk->deactivate();

        if ( mp3list->Size() > 0 )
        {
            playControl( 3 );
        }

        btnNextTrk->activate();
        return;
    }

    if ( w == btnRollUp )
    {
        static unsigned lastClk = GetTickCount();

        unsigned curClk  = GetTickCount();

        if ( ( curClk - lastClk ) < 1000 )
        {
            if ( mainWindow->h() > window_min_h )
            {
                mainWindow->resize( mainWindow->x(),
                                    mainWindow->y(),
                                    mainWindow->w(),
                                    window_min_h );
            }

            lastClk = curClk;
        }

        btnRollUp->hide();

        return;
    }

    if ( w == btnListCtrl )
    {
        if ( mp3list->Size() == 0 )
            return;

        // Create List of MP3.
        if ( grpOverlay->visible_r() == 0 )
        {
            sclMp3List->clear();
            liststrs.clear();

            int lw = sclMp3List->w() - 40;

            for( unsigned cnt=0; cnt<mp3list->Size(); cnt++  )
            {
                Mp3Item* pItem = mp3list->Get( cnt );

                if ( pItem != NULL )
                {
                    Fl_Button* newbtn = new Fl_Button( 0,0,lw,50 );
                    if ( newbtn != NULL )
                    {
                        char tmpstr[128] = {0};

                        sprintf( tmpstr,
                                 "%s\n%s\n%s ( %s )",
                                 pItem->title(),
                                 pItem->artist(),
                                 pItem->album(),
                                 pItem->year() );

                        liststrs.push_back( tmpstr );

                        newbtn->align( FL_ALIGN_INSIDE | FL_ALIGN_CLIP | FL_ALIGN_RIGHT );
                        newbtn->box( FL_NO_BOX );
                        newbtn->labelcolor( 0xFFFFFF00 );
                        newbtn->labelfont( FL_FREE_FONT );
                        newbtn->labelsize( 12 );
                        newbtn->label( liststrs[ liststrs.size() - 1 ].c_str() );
                        newbtn->callback( fl_list_cb, this );

                        sclMp3List->add( newbtn );
                    }
                }
            }

            sclMp3List->autoarrange();

            updateOverlayBG();
            grpOverlay->show();
            boxTitle->stopupdate();
            mainWindow->locksizing();

            return;
        }

    }

    if ( w == mainWindow )
    {
        if ( grpOverlay->visible_r() > 0 )
        {
            grpOverlay->hide();
            boxTitle->continueupdate();
            mainWindow->unlocksizing();

            return;
        }

        fl_message_title( "Program quit" );
        int retask = fl_ask( "%s", "Program may be terminated if you select YES, Proceed it ?" );

        if ( retask > 0 )
        {
            playControl( 1 );

            mainWindow->hide();
            delete mainWindow;
            mainWindow = NULL;
        }

        return;
    }
}

void wMain::MoveCB( Fl_Widget* w )
{
}

void wMain::SizedCB( Fl_Widget* w )
{
    if ( grpOverlay->visible_r() > 0 )
    {
        updateOverlayBG();
        sclMp3List->redraw();
    }

    // Check rollup button visible.
    if ( mainWindow->maximized() == true )
    {
        if ( btnRollUp != NULL )
        {
            if ( btnRollUp->visible_r() > 0 )
            {
                btnRollUp->hide();
            }
        }
    }
    else
    if ( mainWindow->h() > window_min_h )
    {
        if ( btnRollUp != NULL )
        {
            if ( btnRollUp->visible_r() == 0 )
            {
                btnRollUp->show();
            }
        }
    }

    mainWindow->redraw();
}

void wMain::DDropCB( Fl_Widget* w )
{
    const char* testfiles = mainWindow->dragdropfiles();

    if ( testfiles != NULL )
    {
#ifdef DEBUG
        printf("Drag Dropped :\n%s\n", testfiles );
#endif // DEBUG

        string files = testfiles;
        vector<string> dlist = split_string( files, "\n");

        for( unsigned cnt=0; cnt<dlist.size(); cnt++)
        {
#ifdef DEBUG
            printf("Testing dir. : %s\n", dlist[cnt].c_str());
#endif // DEBUG
            if( DirSearch::DirTest( dlist[cnt].c_str() ) == true )
            {
                makePlayList( dlist[cnt].c_str() );
            }
        }

        dlist.clear();
    }
}

void wMain::ListCB( Fl_Widget* w )
{
    unsigned cmax = sclMp3List->children();

    if ( cmax > 2 )
        cmax -= 2;

    for( unsigned cnt=0; cnt<cmax; cnt++ )
    {
        if ( sclMp3List->child( cnt ) == w )
        {
            mainWindow->unlocksizing();
            grpOverlay->hide();
            Fl::wait( 1 );

            playControl( 1 );

            mp3queue = mp3list->FindPlayIndexToListIndex( cnt );

            playControl( 0 );
        }
    }
}

void wMain::DelayedWorkCB()
{
    if ( btnPlayStop != NULL )
    {
        mainWindow->activate();
        btnPlayStop->do_callback();
    }

    Fl::wait( 100 );
    mainWindow->damage( 1 );
    mainWindow->redraw();
}

void wMain::OnNewBuffer( unsigned position, unsigned nbsz, unsigned maxposition )
{
    //printf("audio new buffer at %d + %d bytes \n", position, nbsz );
    if ( skbProgress != NULL )
    {
        skbProgress->range( 1, maxposition );
        skbProgress->currentposition( position );

        unsigned ppos = ( (float)mp3dpos_cur / (float)mp3dpos_max ) * (float)maxposition;
        skbProgress->bufferedposition( ppos );

        skbProgress->update();
        skbProgress->redraw();
    }

    if ( ui_locks == 0 )
    {
        mainWindow->redraw();
        Fl::awake();
    }
}

void wMain::OnBufferEnd()
{
#ifdef DEBUG
    printf("void wMain::OnBufferEnd()\n_mp3controlnext = %d", _mp3controlnext );
#endif // DEBUG
    playControl( 1 );

    // Check for next doing.
    if ( _mp3controlnext == 1 )
    {
        // Next track.
        playControl( 3 );
    }

    if ( ui_locks == 0 )
    {
        mainWindow->redraw();
        Fl::awake();
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void fl_w_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->WidgetCB( w );
    }
}

void fl_list_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->ListCB( w );
    }
}

void fl_move_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->MoveCB( w );
    }
}

void fl_sized_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->SizedCB( w );
    }
}

void fl_ddrop_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->DDropCB( w );
    }
}

void fl_delayed_work_timer_cb( void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->DelayedWorkCB();
    }

    Fl::remove_timeout( fl_delayed_work_timer_cb );
}


void* pthread_play( void* p )
{
    if ( p!= NULL )
    {
        wMain* wm = (wMain*)p;
        return wm->PThreadCall();
    }

    return NULL;
}
