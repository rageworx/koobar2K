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

////////////////////////////////////////////////////////////////////////////////

void fl_w_cb( Fl_Widget* w, void* p );
void fl_list_cb( Fl_Widget* w, void* p );
void fl_move_cb( Fl_Widget* w, void* p );
void fl_sized_cb( Fl_Widget* w, void* p );
void fl_ddrop_cb( Fl_Widget* w, void* p );
void fl_redraw_timer_cb( void* p );
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
   imgWinBG( NULL ),
   imgNoArt( NULL ),
   imgOverlayBg( NULL ),
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

    mp3_frequency = 44100;
    mp3_channels  = 2;

    aout = new AudioDXSound( hParent, this );

    if ( aout != NULL )
    {
        aout->InitAudio( mp3_channels, mp3_frequency );
    }

    // Registering timer.
    Fl::add_timeout( 0.1f, fl_redraw_timer_cb, this );
}

wMain::~wMain()
{

    // Removing timer.
    Fl::remove_timeout( fl_redraw_timer_cb );

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

    if ( mp3list.size() > 0 )
    {
        mainWindow->deactivate();
        Fl::add_timeout( 0.5f, fl_delayed_work_timer_cb, this );
    }

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

    if ( mp3list.size() == 0 )
        return NULL;

    mp3dpos_cur = 0;
    mp3dpos_max = 0;

    unsigned idx = mp3listLUT[ mp3queue ];
    const char* mp3fn = mp3list[ idx ].c_str();

    if ( mp3dec->Open( mp3fn ) == false )
    {
        delete aout;
        return NULL;
    }

    switchPlayButton( 0 );

    // Display progress to first ...
    OnNewBuffer( 1, 10, 10000 );

    loadTags();
    loadArtCover();
    updateOverlayBG();

    while( _threadkillswitch == false )
    {
        unsigned char* abuff = NULL;

        if ( mp3dec != NULL )
        {
            bool isnext = false;
            mp3dec->DecodeFramePos( mp3dpos_cur, mp3dpos_max );
            unsigned abuffsz = mp3dec->DecodeFrame( abuff, &isnext );

            if ( ( mp3_frequency != mp3dec->Frequency() ) ||
                 ( mp3_channels  != mp3dec->Channels() ) )
            {
                mp3_frequency = mp3dec->Frequency();
                mp3_channels  = mp3dec->Channels();

                aout->InitAudio( mp3_channels, mp3_frequency );
            }

            if ( abuffsz > 0 )
            {
                aout->WriteBuffer( abuff, abuffsz );

                if ( _mp3onplaying == false )
                {
                    aout->Control( AudioOut::PLAY, 0, 0 );
                    //bPlayed = true;
                    if ( aout->ControlState() == AudioOut::PLAY )
                    {
                        switchPlayButton( 1 );
                        _mp3onplaying = true;
                    }
                }
            }
            else
            if ( isnext == false )
            {
                mp3dec->Close();
                delete mp3dec;
                mp3dec = NULL;
            }
        }
        else
        {
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
            boxCoverArt->callback( fl_w_cb, this );
        }

        cli_y += test_h;

        skbProgress = new Fl_SeekBar( cli_x + 10 , cli_y, test_w, 3 );

        cli_y += 3;

        boxTrackNo = new Fl_Box( cli_x + 10 , cli_y, test_w, 30 );
        if ( boxTrackNo != NULL )
        {
            boxTrackNo->align( FL_ALIGN_INSIDE | FL_ALIGN_TOP_RIGHT );
            boxTrackNo->labelfont( FL_COURIER_ITALIC );
            boxTrackNo->label( "000/000" );
            boxTrackNo->labelcolor( 0x33333300 );
            boxTrackNo->labelsize( 10 );
        }

        boxTitle = new Fl_Box( cli_x + 10 , cli_y, test_w, 25 );
        if ( boxTitle != NULL )
        {
            boxTitle->label( "No title" );
            boxTitle->labelcolor( 0x33333300 );
            boxTitle->labelfont( FL_FREE_FONT );
            boxTitle->labelsize( 20 );
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

        boxFileInfo = new Fl_Box( cli_x + 10 , cli_y, test_w, 15 );
        if ( boxFileInfo != NULL )
        {
            boxFileInfo->label( "No file information" );
            boxFileInfo->labelcolor( 0x33333300 );
            boxFileInfo->labelfont( FL_FREE_FONT );
            boxFileInfo->labelsize( 12 );
        }

        cli_y += 15;

        // Last empty box for resize NULL container.
        // height = 5
        Fl_Box* testempty = new Fl_Box(  cli_x , cli_y, cli_w, 5 );
        if ( testempty != NULL )
        {
            grpViewer->resizable( testempty );
        }

        cli_y += 5;

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

            //extern unsigned pngimg_ctrl_config_size;
            //extern unsigned char pngimg_ctrl_config[];

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

        //mainWindow->clientarea_sizes( cli_x, cli_y, cli_w, cli_h );
        cli_x = mainWindow->clientarea_x();
        cli_y = mainWindow->clientarea_y();
        cli_w = mainWindow->clientarea_w();
        cli_h = cli_y + mainWindow->clientarea_h();

        grpOverlay = new Fl_Group( cli_x, cli_y, cli_w, cli_h );
        if ( grpOverlay != NULL )
        {
            grpOverlay->box( FL_FLAT_BOX );
            grpOverlay->begin();

            boxOverlayBG = new Fl_Box( cli_x, cli_y, cli_w, cli_h );
            if ( boxOverlayBG != NULL )
            {
                boxOverlayBG->box( FL_NO_BOX );
            }

            cli_x += 5;
            cli_y += 10;
            cli_w -= 10;
            cli_h -= 50;

            sclMp3List = new Fl_NobackScroll( cli_x, cli_y, cli_w, cli_h );
            if ( sclMp3List != NULL )
            {
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

        // put it center of current desktop.
        int dsk_x = 0;
        int dsk_y = 0;
        int dsk_w = 0;
        int dsk_h = 0;

        Fl::screen_work_area( dsk_x, dsk_y, dsk_w, dsk_h );

        int win_p_x = ( ( dsk_w - dsk_x ) - DEF_APP_DEFAULT_W ) / 2;
        int win_p_y = ( ( dsk_h - dsk_y ) - DEF_APP_DEFAULT_H ) / 2;

        mainWindow->position( win_p_x, win_p_y );

        mainWindow->show();
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

    if ( mp3dec->GetTag( "track" , tmpstr, &tmpstrsz) == true )
    {
        if ( tmpstr != NULL )
        {
            strtag_title = (const char*)tmpstr;
            strtag_title += ".";

            free( tmpstr );
        }
    }

    if ( mp3dec->GetTag( "title" , tmpstr, &tmpstrsz) == true )
    {
        if ( tmpstr != NULL )
        {
            strtag_title += (const char*)tmpstr;

            free( tmpstr );
        }

        boxTitle->label( strtag_title.c_str() );
    }

    if ( mp3dec->GetTag( "album" , tmpstr, &tmpstrsz) == true )
    {
        if ( tmpstr != NULL )
        {
            strtag_album = (const char*)tmpstr;

            free( tmpstr );
        }
    }

    if ( mp3dec->GetTag( "year", tmpstr, &tmpstrsz ) == true )
    {
        if ( tmpstr != NULL )
        {
            if ( strtag_album.size() > 0 )
            {
                strtag_album += " (";
                strtag_album += (const char*)tmpstr;
                strtag_album += ")";
            }
            else
            {
                strtag_album += (const char*)tmpstr;
            }

            free( tmpstr );
        }
    }


    if ( mp3dec->GetTag( "artist" , tmpstr, &tmpstrsz) == true )
    {
        if ( tmpstr != NULL )
        {
            strtag_artist = (const char*)tmpstr;

            free( tmpstr );
        }
    }

    strtag_moreinfo.clear();

    if ( mp3dec->GetTag( "genre", tmpstr, &tmpstrsz ) == true )
    {
        if ( tmpstr != NULL )
        {
            strtag_moreinfo += (const char*)tmpstr;

            free( tmpstr );
        }
    }

    boxAlbum->label( strtag_album.c_str() );
    boxArtist->label( strtag_artist.c_str() );
    boxMiscInfo->label( strtag_moreinfo.c_str() );

    updateInfo();
}

void wMain::updateTrack()
{
    char tmpmap[80] = {0};

    strinf_trackno.clear();

    if ( mp3list.size() > 0 )
    {
        sprintf( tmpmap, "%03d/%03d", mp3queue + 1, mp3list.size() );
        strinf_trackno = tmpmap;
    }
    else
    {
        strinf_trackno = "000/000";
    }
    boxTrackNo->label( strinf_trackno.c_str() );
}

void wMain::updateInfo()
{
    updateTrack();

    char tmpmap[80] = {0};

    strtag_fileinfo.clear();

    char* strstro[] = { "stereo", "joint-stereo", "dual", "mono" };
    char* strbrs[] = { "CBR", "VBR", "ABR" };

    sprintf( tmpmap, "%s - %.1fKHz - %s - %dKbps - mpeg.l%d",
             strstro[ mp3dec->Mode() ],
             (float)mp3dec->Frequency() / (float)1000,
             strbrs[ mp3dec->BRtype() ],
             mp3dec->BitRate(),
             mp3dec->Layer() );

    strtag_fileinfo = tmpmap;

    boxFileInfo->label( strtag_fileinfo.c_str() );
}

void wMain::updateOverlayBG()
{
    static bool uobgmutexd = false;

    if ( uobgmutexd == true )
        return;

    uobgmutexd = true;

    Fl::lock();

    grpViewer->damage( 1 );
    grpViewer->redraw();

    if ( imgOverlayBg != NULL )
    {
        fl_imgtk::discard_user_rgb_image( imgOverlayBg );
    }

    imgOverlayBg = fl_imgtk::drawblurred_widgetimage( grpViewer,
                                                      grpViewer->w() / 50 );

    Fl::unlock();

    if ( imgOverlayBg != NULL )
    {
        fl_imgtk::brightbess_ex( imgOverlayBg, -50.f );
    }

    boxOverlayBG->image( imgOverlayBg );
    boxOverlayBG->redraw();

    if( grpOverlay->visible_r() > 0 )
    {
        grpOverlay->redraw();
    }

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

    boxCoverArt->image( imgNoArt );

    _mp3art_loaded = false;
}

void wMain::switchPlayButton( int state )
{
    if ( state > 1 )
        return;

    if ( btnPlayStop != NULL )
    {
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
    }
}

void wMain::makePlayList( const char* refdir )
{
    string dirpath;

    if ( refdir != NULL )
    {
        dirpath = refdir;

        DirSearch* dsearch = new DirSearch( dirpath.c_str(), ".mp3" );
        if ( dsearch != NULL )
        {
            vector< string >* plist = dsearch->data();
            if ( plist->size() > 0 )
            {
                for( unsigned cnt=0; cnt<plist->size(); cnt++ )
                {
                    string item  = plist->at( cnt );
                    vector< string >::const_iterator it;

                    it = find( mp3list.begin(), mp3list.end(), item.c_str() );
                    if ( it == mp3list.end() )
                    {
                        mp3list.push_back( item );
                    }
                }
            }

            delete dsearch;
        }
    }

    // Let make shuttle LUT !
    if ( mp3list.size() > 0 )
    {
        mp3listLUT.clear();
        mp3listLUT.resize( mp3list.size() );

        for( unsigned cnt=0; cnt<mp3list.size(); cnt++ )
        {
            mp3listLUT[ cnt ] = cnt;
        }

        std::random_shuffle( mp3listLUT.begin(), mp3listLUT.end() );
    }

    updateTrack();

    mp3queue = 0;
}

void wMain::playControl( int action )
{
    if ( action > 3 )
        return;

    switch( action )
    {
        case 0 : /// PLAY
            {
                if ( mp3list.size() > 0 )
                {
                    if ( ( _mp3onplaying == false ) && ( _pt == NULL ) )
                    {
                        int ret = pthread_create( &_pt, NULL, pthread_play, this );
                    }
                }
            }
            break;

        case 1: /// STOP.
            {
                if ( mp3list.size() > 0 )
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

                            _threadkillswitch = false;
                            _mp3onplaying = false;
                        }
                    }
                }
            }
            break;

        case 2: /// Previous Track
            {
                if ( mp3list.size() > 0 )
                {
                    playControl( 1 );
                    Sleep( 500 );

                    if ( mp3queue == 0 )
                    {
                        mp3queue = mp3list.size() - 1;
                    }
                    else
                    {
                        mp3queue--;
                    }

                    playControl( 0 );
                }
            }
            break;

        case 3: /// Next Track
            {
                if ( mp3list.size() > 0 )
                {
                    playControl( 1 );
                    Sleep( 500 );

                    mp3queue++;

                    if( mp3queue >= mp3list.size() )
                    {
                        mp3queue = 0;
                    }

                    playControl( 0 );
                }
            }
            break;
    }
}

void wMain::loadArtCover()
{
    if ( mp3dec == NULL )
        return;

    uchar* img;
    unsigned imgsz;

    if ( mp3dec->GetTag( "art", img, &imgsz ) == false )
        return;

    if ( ( img == NULL ) || ( imgsz == 0 ) )
    {
        if ( img != NULL )
            free( img );

        return;
    }

    extern unsigned artmasksz;
    extern unsigned char artmask[];


    Fl_PNG_Image* amaskimg = new Fl_PNG_Image( "artmask", artmask, artmasksz );
    Fl_JPEG_Image* coverimg = new Fl_JPEG_Image( "art", img );

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
        mainWindow->bgimage( NULL );

        fl_imgtk::discard_user_rgb_image( imgWinBG );

        // Get Maximum size
        unsigned maxwh = max( mainWindow->w(), mainWindow->h() );

        imgWinBG = fl_imgtk::rescale( (Fl_RGB_Image*)coverimg,
                                      maxwh, maxwh,
                                      fl_imgtk::NONE );

        if ( imgWinBG != NULL )
        {
            fl_imgtk::blurredimage_ex( imgWinBG, 10 );
            fl_imgtk::brightbess_ex( imgWinBG, -80.f );

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

            mainWindow->refreshcontrolbuttonsimages();

            mainWindow->redraw();

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

            boxCoverArt->image( img );
            boxCoverArt->redraw();
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

void wMain::requestWindowRedraw()
{
    Fl::repeat_timeout( 0.05f, fl_redraw_timer_cb, this );
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

        if ( mp3list.size() > 0 )
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

        if ( mp3list.size() > 0 )
        {
            playControl( 2 );
        }

        btnPrevTrk->activate();
        return;
    }

    if ( w == btnNextTrk )
    {
        btnNextTrk->deactivate();

        if ( mp3list.size() > 0 )
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

        return;
    }

    if ( w == btnListCtrl )
    {
        if ( grpOverlay->visible_r() == 0 )
        {
            sclMp3List->clear();

            int lw = sclMp3List->w() - 40;

            for( unsigned cnt=0; cnt<mp3list.size(); cnt++  )
            {
                Fl_Button* newbtn = new Fl_Button( 0,0,lw,20, mp3list[cnt].c_str() );
                if ( newbtn != NULL )
                {
                    newbtn->align( FL_ALIGN_INSIDE | FL_ALIGN_CLIP | FL_ALIGN_RIGHT );
                    newbtn->color( 0x33333300 );
                    newbtn->labelcolor( 0xFFFFFF00 );
                    newbtn->labelfont( FL_FREE_FONT );
                    newbtn->labelsize( 11 );
                    sclMp3List->add( newbtn );
                    newbtn->callback( fl_list_cb, this );
                }
            }

            sclMp3List->autoarrange();

            updateOverlayBG();
            grpOverlay->show();

            return;
        }

    }

    if ( w == mainWindow )
    {
        if ( grpOverlay->visible_r() > 0 )
        {
            grpOverlay->hide();

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
    }
}

void wMain::DDropCB( Fl_Widget* w )
{
    const char* testfiles = mainWindow->dragdropfiles();

    if ( testfiles != NULL )
    {
        string files = testfiles;
        vector<string> dlist = split_string( files, "\n");

        for( unsigned cnt=0; cnt<dlist.size(); cnt++)
        {
            printf("%s\n", dlist[cnt].c_str());
            if( DirSearch::DirTest( dlist[cnt].c_str() ) == true )
            {
                makePlayList( dlist[cnt].c_str() );
            }
        }
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
            playControl( 1 );

            //find LUT
            int lutpos = -1;

            for( unsigned c2=0; c2<mp3listLUT.size(); c2++ )
            {
                if ( mp3listLUT[ c2 ] == cnt )
                {
                    lutpos = c2;
                    break;
                }
            }

            grpOverlay->hide();

            if ( lutpos >= 0 )
            {
                mp3queue = lutpos;
                playControl( 0 );
            }
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
        mainWindow->redraw();
        requestWindowRedraw();
    }
}

void wMain::OnBufferEnd()
{
    playControl( 1 );

    // Check for next doing.
    if ( _mp3controlnext == 1 )
    {
        // Next track.
        playControl( 2 );
    }

    requestWindowRedraw();
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

void fl_redraw_timer_cb( void* p )
{
    // Simple mutex.
    static bool isflushing = false;

    if ( isflushing == false )
    {
        isflushing = true;

        if ( p != NULL )
        {
            Fl::awake();
        }

        isflushing = false;
    }
    else
    {
        Fl::repeat_timeout( 0.05f, fl_redraw_timer_cb, p );
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
