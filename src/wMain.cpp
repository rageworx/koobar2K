#ifdef _WIN32
    #include <windows.h>
#endif // of _WIN32

#include <unistd.h>
#include <dirent.h>

#include <cstdio>
#include <cstring>

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
void fl_move_cb( Fl_Widget* w, void* p );

void* pthread_work( void* p );

extern void getartmask( unsigned char* &data, unsigned &sz );

////////////////////////////////////////////////////////////////////////////////

wMain::wMain( int argc, char** argv )
 : _argc( argc ),
   _argv( argv ),
   _runsatfullscreen( false ),
   _keyprocessing( false ),
   _firstthreadrun( true ),
   //testswitch( NULL ),
   winbgimg( NULL ),
   aout( NULL )
{
    parseParams();
    createComponents();

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

    aout = new AudioDXSound( hParent, this );

    if ( aout != NULL )
    {
        aout->InitAudio( 2, 44100 );
    }
}

wMain::~wMain()
{
    if ( aout != NULL )
    {
        aout->Control( AudioOut::STOP, 0, 0 );
        aout->FinalAudio();

        delete aout;
    }
}

int wMain::Run()
{
    _threadkillswitch = false;

    // Wait for window has a handle ...

    string dirpath = "./test";

    if ( _argc > 1 )
    {
        if ( DirSearch::DirTest( _argv[1] ) == true )
        {
            dirpath = _argv[1];
        }
    }

    DirSearch* dsearch = new DirSearch( dirpath.c_str(), ".mp3" );
    if ( dsearch != NULL )
    {
        vector< string >* plist = dsearch->data();
        if ( plist->size() > 0 )
        {
            for( unsigned cnt=0; cnt<plist->size(); cnt++ )
            {
                mp3list.push_back( plist->at( cnt ) );
            }
        }

        delete dsearch;

        if ( mp3list.size() > 0 )
        {
            mp3queue = GetTickCount() % mp3list.size();

            int ret = pthread_create( &_pt, NULL, pthread_work, this );
        }
    }

    return Fl::run();
}

void* wMain::PThreadCall()
{
    if ( aout == NULL )
        return NULL;

    if ( _firstthreadrun == true )
    {
        _firstthreadrun = false;
        Sleep( 100 );
        Fl::flush();
    }

    mp3dec = new MPG123Wrap();

    if ( mp3dec == NULL )
        return NULL;

    if ( mp3list.size() == 0 )
        return NULL;

    mp3dpos_cur = 0;
    mp3dpos_max = 0;

    const char* mp3fn = mp3list[ mp3queue ].c_str();

    if ( mp3dec->Open( mp3fn ) == false )
    {
        delete aout;
        return NULL;
    }

    // Display progress to first ...
    OnNewBuffer( 1, 10, 100 );

    bool bPlayed = false;

    loadTags();
    loadArtCover();

    mainWindow->redraw();

    while( _threadkillswitch == false )
    {
        unsigned char* abuff = NULL;

        if ( mp3dec != NULL )
        {
            bool isnext = false;
            mp3dec->DecodeFramePos( mp3dpos_cur, mp3dpos_max );
            unsigned abuffsz = mp3dec->DecodeFrame( abuff, &isnext );

            if ( abuffsz > 0 )
            {
                aout->WriteBuffer( abuff, abuffsz );

                if ( bPlayed == false )
                {
                    aout->Control( AudioOut::PLAY, 0, 0 );
                    //bPlayed = true;
                    if ( aout->ControlState() == AudioOut::PLAY )
                    {
                        bPlayed = true;
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
        int cli_h = mainWindow->clientarea_h();

        winbgimg = fl_imgtk::makegradation_h( DEF_APP_DEFAULT_W, DEF_APP_DEFAULT_H,
                                              0xCCCCCC00, 0x99999900, true );

        mainWindow->bgimage( winbgimg, FL_ALIGN_CENTER );
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

        boxTrackNo = new Fl_Box( cli_x + 10 , cli_y, test_w, 30 );
        if ( boxTrackNo != NULL )
        {
            boxTrackNo->align( FL_ALIGN_INSIDE | FL_ALIGN_TOP_RIGHT );
            boxTrackNo->labelfont( FL_COURIER_ITALIC );
            boxTrackNo->label( "000/000" );
            boxTrackNo->labelcolor( 0x33333300 );
            boxTrackNo->labelsize( 10 );
        }

        boxTitle = new Fl_Box( cli_x + 10 , cli_y, test_w, 30 );
        if ( boxTitle != NULL )
        {
            boxTitle->label( "No title" );
            boxTitle->labelcolor( 0x33333300 );
            boxTitle->labelfont( FL_FREE_FONT );
            boxTitle->labelsize( 20 );
        }

        cli_y += 30;

        boxAlbum = new Fl_Box( cli_x + 10 , cli_y, test_w, 20 );
        if ( boxAlbum != NULL )
        {
            boxAlbum->label( "No album" );
            boxAlbum->labelcolor( 0x33333300 );
            boxAlbum->labelfont( FL_FREE_FONT );
            boxAlbum->labelsize( 14 );
        }

        cli_y += 20;

        boxArtist = new Fl_Box( cli_x + 10 , cli_y, test_w, 20 );
        if ( boxArtist != NULL )
        {
            boxArtist->label( "No artist" );
            boxArtist->labelcolor( 0x33333300 );
            boxArtist->labelfont( FL_FREE_FONT );
            boxArtist->labelsize( 14 );
        }

        cli_y += 20;

        boxMiscInfo = new Fl_Box( cli_x + 10 , cli_y, test_w, 20 );
        if ( boxMiscInfo != NULL )
        {
            boxMiscInfo->label( "No more information" );
            boxMiscInfo->labelcolor( 0x33333300 );
            boxMiscInfo->labelfont( FL_FREE_FONT );
            boxMiscInfo->labelsize( 12 );
        }

        cli_y += 20;

        boxFileInfo = new Fl_Box( cli_x + 10 , cli_y, test_w, 20 );
        if ( boxFileInfo != NULL )
        {
            boxFileInfo->label( "No file information" );
            boxFileInfo->labelcolor( 0x33333300 );
            boxFileInfo->labelfont( FL_FREE_FONT );
            boxFileInfo->labelsize( 12 );
        }

        cli_y += 20;

        // Last empty box for resize NULL container.
        cli_y += 20;

        Fl_Box* testempty = new Fl_Box(  cli_x , cli_y, cli_w, cli_h - cli_y );
        if ( testempty != NULL )
        {
            grpViewer->resizable( testempty );
        }

        // --------------------------


        if ( grpViewer != NULL )
        {
            grpViewer->end();
            mainWindow->clientarea()->resizable( grpViewer );
        }

        grpOverlay = new Fl_Group( cli_x, cli_y, cli_w, cli_h );
        if ( grpOverlay != NULL )
        {
            grpOverlay->box( FL_FLAT_BOX );
            grpOverlay->begin();

            int pp_w = cli_w / 2;
            int pp_h = cli_h / 2;
            int pp_x = ( cli_w - pp_w ) / 2;
            int pp_y = ( cli_h - pp_h ) / 2;

            Fl_Box* boxtest = new Fl_Box( cli_x + pp_x , cli_y + pp_y, pp_w, pp_h, "A\nTEST BOX\nHERE !" );
            if ( boxtest != NULL )
            {
                boxtest->box( FL_NO_BOX );
                boxtest->color( FL_WHITE );
            }

            grpOverlay->end();
            grpOverlay->hide();
        }

        mainWindow->end();
        mainWindow->callback( fl_w_cb, this );
        mainWindow->callback_onmove( fl_move_cb, this );

        int min_w_h = ( DEF_APP_DEFAULT_H  / 3 ) * 2;

        mainWindow->size_range( DEF_APP_DEFAULT_W, min_w_h, DEF_APP_DEFAULT_W );

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

void wMain::updateInfo()
{
    char tmpmap[80] = {0};

    strinf_trackno.clear();

    sprintf( tmpmap, "%03d/%03d", mp3queue + 1, mp3list.size() );
    strinf_trackno = tmpmap;
    boxTrackNo->label( strinf_trackno.c_str() );

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

    unsigned artmasksz;
    unsigned char* artmask = NULL;

    getartmask( artmask, artmasksz );

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

        fl_imgtk::discard_user_rgb_image( winbgimg );

        // Get Maximum size
        unsigned maxwh = max( mainWindow->w(), mainWindow->h() );

        winbgimg = fl_imgtk::rescale( (Fl_RGB_Image*)coverimg,
                                      maxwh, maxwh,
                                      fl_imgtk::NONE );

        if ( winbgimg != NULL )
        {
            fl_imgtk::blurredimage_ex( winbgimg, 10 );
            fl_imgtk::brightbess_ex( winbgimg, -80.f );

            mainWindow->bgimage( winbgimg, FL_ALIGN_CENTER );

            // No need get illumination, it always draws bright color.
            Fl_Color newcol = 0xCFCFCFFF;

            mainWindow->labelcolor( newcol );
            boxArtist->labelcolor( newcol );
            boxAlbum->labelcolor( newcol );
            boxTitle->labelcolor( newcol );
            boxMiscInfo->labelcolor( newcol );
            boxFileInfo->labelcolor( newcol );

            boxTrackNo->labelcolor( fl_darker( newcol ) );

            for( unsigned cnt=0; cnt<4; cnt++ )
            {
                Fl_Image* pimg = mainWindow->controlbuttonsimage( cnt );
                if ( pimg != NULL )
                {
                    fl_imgtk::invert_ex( (Fl_RGB_Image*)pimg );
                }
            }

            mainWindow->refreshcontrolbuttonsimages();

            mainWindow->redraw();
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
    if ( w == mainWindow )
    {
        fl_message_title( "Program quit" );
        int retask = fl_ask( "%s", "Program may be terminated if you select YES, Proceed it ?" );

        if ( retask > 0 )
        {
            // Wait for thread ends.

            if ( ( _pt != NULL ) && ( _threadkillswitch == false ) )
            {
                _threadkillswitch = true;
                printf("Wait for thread ends ...");
                //pthread_kill( _pt, 0 );
                pthread_join( _pt, 0 );
                printf("Ok.\n");
            }

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
        Fl::flush();
    }
}

void wMain::OnBufferEnd()
{
    if ( mp3dec != NULL )
    {
        mp3dec->Close();
    }

    if ( aout != NULL )
    {
        aout->Control( AudioOut::STOP, 0, 0 );
    }

    //printf("audio buffer finished.\n");
    if ( ( _pt != NULL ) && ( _threadkillswitch == false ) )
    {
        _threadkillswitch = true;
        //pthread_kill( _pt, 0 );
        pthread_join( _pt, 0 );
        _pt = NULL;
    }

    if ( mp3list.size() > 0 )
    {
        mp3queue++;
        if ( mp3queue >= mp3list.size() )
        {
            mp3queue = 0;
        }

        _threadkillswitch = false;

        int ret = pthread_create( &_pt, NULL, pthread_work, this );
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

void fl_move_cb( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        wMain* wm = (wMain*)p;
        wm->MoveCB( w );
    }
}

void* pthread_work( void* p )
{
    if ( p!= NULL )
    {
        wMain* wm = (wMain*)p;
        return wm->PThreadCall();
    }

    return NULL;
}
