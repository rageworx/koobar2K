#ifdef DEBUG
    #ifndef _WIN32
        #define _WIN32
    #endif
#endif

#ifdef _WIN32
    #include <windows.h>
#endif // _WIN32

#include <sys/time.h>

#include <FL/fl_draw.H>
#include "Fl_BorderlessWindow.H"
#include "fl_imgtk.h"

#include <algorithm>
#include <cmath>

using namespace std;

////////////////////////////////////////////////////////////////////////////////

#define DEF_FL_BORDERLESSWINDOW_ICON_BOX_WH         20
#define DEF_FL_BORDERLESSWINDOW_LABEL_SIZE          12
#define DEF_FL_BWINDOW_SYS_BTN_TEXT_HIDE            "@8<"
#define DEF_FL_BWINDOW_SYS_BTN_TEXT_MAXIMIZE        "@2<"
#define DEF_FL_BWINDOW_SYS_BTN_TEXT_RETURNSIZE      "@undo"
#define DEF_FL_BWINDOW_SYS_BTN_TEXT_CLOSE           "@1+"


#ifndef _MIN
    #define _MIN(a,b)    (((a)<(b))?(a):(b))
#endif

#ifndef _MAX
    #define _MAX(a,b)    (((a)>(b))?(a):(b))
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
Fl_RGB_Image* icon_to_flrgb(HICON hIcon)
{
    BITMAP bm;
	ICONINFO iconInfo;

	GetIconInfo(hIcon, &iconInfo);
	GetObject(iconInfo.hbmColor, sizeof(BITMAP),&bm);

	int width = bm.bmWidth;
	int height = bm.bmHeight;
	int bytesPerScanLine = (width * 3 + 3) & 0xFFFFFFFC;

	BITMAPINFO infoheader = {0};
	infoheader.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	infoheader.bmiHeader.biWidth    = width;
	infoheader.bmiHeader.biHeight   = height;
	infoheader.bmiHeader.biPlanes   = 1;
	infoheader.bmiHeader.biBitCount = 24;
	infoheader.bmiHeader.biCompression = BI_RGB;
	infoheader.bmiHeader.biSizeImage = bytesPerScanLine * height;

	// allocate Memory for Icon RGB data plus Icon mask plus ARGB buffer for the resulting image
	unsigned char* pixelsIconRGB = new unsigned char[ height * width * 3 ];

	if ( pixelsIconRGB == NULL )
    {
        return NULL;
    }

	unsigned char* alphaPixels   = new unsigned char[ height * width * 3 ];

    if ( alphaPixels == NULL )
    {
        delete[] pixelsIconRGB;

        return NULL;
    }

	unsigned char* imagePixels   = new unsigned char[ height * width * 4 ];

    if ( imagePixels == NULL )
    {
        delete[] pixelsIconRGB;
        delete[] alphaPixels;

        return NULL;
    }

	HDC hDC = CreateCompatibleDC(NULL);

	// Get Icon RGB data
	HBITMAP hBmpOld = (HBITMAP)SelectObject(hDC, (HGDIOBJ)iconInfo.hbmColor);

	if( GetDIBits( hDC, iconInfo.hbmColor,
                   0, height, (LPVOID) pixelsIconRGB,
                   &infoheader, DIB_RGB_COLORS) == 0 )
    {
        DeleteDC(hDC);

        delete[] pixelsIconRGB;
        delete[] alphaPixels;
        delete[] imagePixels;

        return NULL;
    }

	SelectObject( hDC, hBmpOld );

    // now get alpha channel.
	if( GetDIBits( hDC, iconInfo.hbmMask,
                   0, height,(LPVOID)alphaPixels,
                   &infoheader, DIB_RGB_COLORS ) == 0 )
    {
        DeleteDC(hDC);

        delete[] pixelsIconRGB;
        delete[] alphaPixels;
        delete[] imagePixels;

        return NULL;
    }

	DeleteDC(hDC);

	int x=0;
	int currentSrcPos=0;
	int currentDestPos=0;
	int linePosSrc = 0;
	int linePosDest = 0;
	int vsDest = height-1;

	for(int y=0; y<height; y++)
	{
		linePosSrc  = ( vsDest - y ) * ( width * 3 );
		linePosDest = y * width * 4;

		for(x=0; x<width; x++)
		{
			currentDestPos = linePosDest + ( x * 4 );
			currentSrcPos  = linePosSrc + ( x * 3);

			imagePixels[ currentDestPos + 0 ] = pixelsIconRGB[ currentSrcPos + 2 ];
			imagePixels[ currentDestPos + 1 ] = pixelsIconRGB[ currentSrcPos + 1 ];
			imagePixels[ currentDestPos + 2 ] = pixelsIconRGB[ currentSrcPos + 0 ];

			unsigned a_sum = alphaPixels[ currentSrcPos + 2 ];
			a_sum += alphaPixels[ currentSrcPos + 1 ];
			a_sum += alphaPixels[ currentSrcPos + 0 ];
			a_sum  = 255 - ( a_sum / 3 );

			imagePixels[ currentDestPos + 3 ] = a_sum;
		}
	}

	Fl_RGB_Image* pImage = new Fl_RGB_Image( imagePixels, width, height, 4 );

    delete[] pixelsIconRGB;
    delete[] alphaPixels;

	return pImage;
}
#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////

Fl_BorderlessWindowTitle::Fl_BorderlessWindowTitle( int X, int Y, int W, int H, const char* T )
 : Fl_Box( X, Y, W, H, T ),
   bwin( NULL ),
   grabbed( false ),
   dblclk_pre_x( 0 ),
   dblclk_pre_y( 0 ),
   dblclk_nxt_x( 0 ),
   dblclk_nxt_y( 0 ),
   dblclk_pre_t( 0 ),
   dblclk_nxt_t( 0 )
{
    align( FL_ALIGN_LEFT | FL_ALIGN_CLIP | FL_ALIGN_INSIDE );
}

Fl_BorderlessWindowTitle::~Fl_BorderlessWindowTitle()
{
}

bool Fl_BorderlessWindowTitle::doubleclicked()
{
    unsigned cur_t = getcurrenttimems();

    if ( ( dblclk_pre_x == dblclk_nxt_x ) && ( dblclk_pre_y == dblclk_nxt_y ) )
    {
        if ( ( cur_t - dblclk_nxt_t ) < 500 )
        {
            unsigned diff_t = dblclk_nxt_t - dblclk_pre_t;
            if ( diff_t < 1000 )
            {
                return true;
            }
        }
    }

    return false;
}

int Fl_BorderlessWindowTitle::handle( int e )
{
    switch ( e )
    {
        case FL_PUSH:
            if ( bwin != NULL )
            {
                int mbtn = Fl::event_button();

                if ( mbtn == FL_LEFT_MOUSE )
                {
                    if ( grabbed == false )
                    {
                        grabbed = true;

                        Fl::get_mouse( grab_x, grab_y );

                        //grab_x += bwin->x();
                        //grab_y += bwin->y();

                        grab_x -= bwin->x();
                        grab_y -= bwin->y();

                        return 1;
                    }
                }
            }
            break;

        case FL_LEAVE:
            if ( bwin != NULL )
            {
                if ( grabbed == true )
                {
                    grabbed = false;

                    return 1;
                }
            }
            break;

        case FL_RELEASE:
            if ( bwin != NULL )
            {
                int mbtn = Fl::event_button();

                if ( mbtn > 0 )
                {
                    if ( grabbed == true )
                    {
                        grabbed = false;

                        if ( mbtn = FL_LEFT_MOUSE )
                        {
                            dblclk_pre_x = dblclk_nxt_x;
                            dblclk_pre_y = dblclk_nxt_y;
                            dblclk_pre_t = dblclk_nxt_t;

                            dblclk_nxt_t = getcurrenttimems();
                            Fl::get_mouse( dblclk_nxt_x, dblclk_nxt_y );

                            if ( doubleclicked() == true )
                            {
                                bwin->procdblclktitle();
                            }
                        }

                        return 1;
                    }
                }
            }
            break;

        case FL_DRAG:
            if ( bwin != NULL )
            {
                if ( grabbed == true )
                {
                    int cur_x = 0;
                    int cur_y = 0;

                    Fl::get_mouse( cur_x, cur_y );

                    if ( bwin != NULL )
                    {
                        bwin->procwindowmove( cur_x - grab_x, cur_y - grab_y );

                        return 1;
                    }
                }
            }
            break;
    }

    return Fl_Box::handle( e );
}

unsigned Fl_BorderlessWindowTitle::getcurrenttimems()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Fl_BorderlessWindow::Fl_BorderlessWindow( int W, int H, const char* T )
 : Fl_Double_Window( W, H, T ),
   boxWindowIcon( NULL ),
   boxWindowTitle( NULL ),
   grpInnerWindow( NULL ),
   sizegrip_m( 5 ),
   sizegripped( false ),
   maximized_wh( false ),
   singlewindowtype( 0 ),
   cbOnMove( NULL ),
   defaultboxtype( FL_NO_BOX ),
   min_w( 0 ),
   min_h( 0 ),
   max_w( 0 ),
   max_h( 0 ),
   backgroundimg( NULL ),
   convtitleiconimg( NULL ),
   backgroundimgcached( NULL )
{
    boxWindowButtons[0] = NULL;
    boxWindowButtons[1] = NULL;
    boxWindowButtons[2] = NULL;

    Fl_Double_Window::border( 0 );
    Fl_Double_Window::box( FL_THIN_UP_BOX );

    // Create inner components.
    Fl_Double_Window::begin();

    int box_wh = DEF_FL_BORDERLESSWINDOW_ICON_BOX_WH;

    Fl_Group* fbwGroupTitleDivAll = new Fl_Group( 0, 0, W, box_wh );
    if ( fbwGroupTitleDivAll != NULL )
    {
        fbwGroupTitleDivAll->begin();
    }

    Fl_Group* fbwGroupTitleDiv0 = new Fl_Group( 0, 0, box_wh, box_wh );
    if ( fbwGroupTitleDiv0 != NULL )
    {
        fbwGroupTitleDiv0->begin();
    }

    boxWindowIcon = new Fl_Box( 0, 0, box_wh, box_wh );
    if ( boxWindowIcon != NULL )
    {
        boxWindowIcon->box( defaultboxtype );
        boxWindowIcon->labelsize( DEF_FL_BORDERLESSWINDOW_LABEL_SIZE );
    }

    if ( fbwGroupTitleDiv0 != NULL )
    {
        fbwGroupTitleDiv0->end();
    }

    int box_l = W - ( DEF_FL_BORDERLESSWINDOW_ICON_BOX_WH * 3 );

    Fl_Group* fbwGroupTitleDiv2 = new Fl_Group( box_l, 0, box_wh * 3, box_wh );
    if ( fbwGroupTitleDiv2 != NULL )
    {
        fbwGroupTitleDiv2->begin();
    }

    const char* box_l_order[3] = { DEF_FL_BWINDOW_SYS_BTN_TEXT_HIDE,
                                   DEF_FL_BWINDOW_SYS_BTN_TEXT_MAXIMIZE,
                                   DEF_FL_BWINDOW_SYS_BTN_TEXT_CLOSE };
    for( int cnt=0; cnt<3; cnt++ )
    {
        boxWindowButtons[cnt] = new Fl_Button( box_l, 0, box_wh, box_wh, box_l_order[cnt] );
        if ( boxWindowButtons[cnt] != NULL )
        {
            boxWindowButtons[cnt]->box( defaultboxtype );
            boxWindowButtons[cnt]->labelsize( DEF_FL_BORDERLESSWINDOW_LABEL_SIZE );
            boxWindowButtons[cnt]->clear_visible_focus();
            boxWindowButtons[cnt]->callback( Fl_BorderlessWindow::Fl_BorderlessWindow_CB, this );
        }

        box_l += box_wh;
    }

    if ( fbwGroupTitleDiv2 != NULL )
    {
        fbwGroupTitleDiv2->end();
    }

    int title_w = W - ( DEF_FL_BORDERLESSWINDOW_ICON_BOX_WH * 3 ) - box_wh;

    Fl_Group* fbwGroupTitleDiv1 = new Fl_Group( box_wh, 0, title_w, box_wh );
    if ( fbwGroupTitleDiv1 != NULL )
    {
        fbwGroupTitleDiv1->begin();
    }

    boxWindowTitle = new Fl_BorderlessWindowTitle( box_wh, 0, title_w, box_wh, T );
    if ( boxWindowTitle != NULL )
    {
        boxWindowTitle->borderlesswindow( this );
        boxWindowTitle->box( defaultboxtype );
        boxWindowTitle->labelsize( DEF_FL_BORDERLESSWINDOW_LABEL_SIZE );
    }

    if ( fbwGroupTitleDiv1 != NULL )
    {
        fbwGroupTitleDiv1->end();
    }

    if ( fbwGroupTitleDivAll != NULL )
    {
        fbwGroupTitleDivAll->end();

        if ( fbwGroupTitleDiv1 != NULL )
        {
            fbwGroupTitleDivAll->resizable( fbwGroupTitleDiv1 );
        }
    }

    grpInnerWindow = new Fl_Group( 1, box_wh + 1, W - 2, H - box_wh - 2 );
    if ( grpInnerWindow != NULL )
    {
        grpInnerWindow->align( FL_ALIGN_CLIP | FL_ALIGN_INSIDE );
        this->resizable( grpInnerWindow );
    }

    Fl_Double_Window::end();

    begin();
}

Fl_BorderlessWindow::~Fl_BorderlessWindow()
{
    releaseconvtitleiconimg();

    if ( backgroundimgcached != NULL )
    {
        fl_imgtk::discard_user_rgb_image( backgroundimgcached );
    }
}

void Fl_BorderlessWindow::begin()
{
    if ( grpInnerWindow != NULL )
    {
        grpInnerWindow->begin();
    }
}

void Fl_BorderlessWindow::end()
{
    if ( grpInnerWindow != NULL )
    {
        grpInnerWindow->end();
    }
}

void Fl_BorderlessWindow::size_range(int minw, int minh, int maxw, int maxh)
{
    if ( minw > 0 )
        min_w = minw;

    if ( minh > 0 )
        min_h = minh;

    if ( ( maxw > minw ) || ( maxw == 0 ) )
        max_w = maxw;
    else
        max_w = min_w;

    if ( ( maxh >= minh ) || ( maxh == 0 ) )
        max_h = maxh;
    else
        max_h = min_h;
}

void Fl_BorderlessWindow::draw()
{
    if ( backgroundfitting == false )
    {
        if ( backgroundimg != NULL )
        {
            backgroundimg->draw(0,0);
        }
    }
    else
    if ( backgroundimgcached != NULL )
    {
        backgroundimgcached->draw( 0, 0 );
    }
    else
    if ( backgroundimg != NULL )
    {
        backgroundimg->draw(0,0);
    }

    Fl_Double_Window::draw();
}

void Fl_BorderlessWindow::procdblclktitle()
{
    if ( boxWindowButtons[1] != NULL )
    {
        boxWindowButtons[1]->do_callback();
    }
}

int Fl_BorderlessWindow::sizegriptest( int x, int y )
{
    int reti = 0;

    if ( min_w != max_w )
    {
        if ( ( x >= w() - sizegrip_m ) && ( x <= w() ) )
        {
            reti |= 1;
        }
    }

    if ( min_h != max_h )
    {
        if ( ( y >= h() - sizegrip_m ) && ( y <= h() ) )
        {
            reti |= 2;
        }
    }

    return reti;
}

void Fl_BorderlessWindow::titleicon( Fl_Image* i )
{
    if ( boxWindowIcon != NULL )
    {
        boxWindowIcon->image( i );
    }
}

void Fl_BorderlessWindow::bgimage( Fl_Image* i, bool fitting )
{
    backgroundimg = i;
    backgroundfitting = fitting;

    if ( backgroundimgcached != NULL )
    {
        fl_imgtk::discard_user_rgb_image( backgroundimgcached );
    }
}

#ifdef _WIN32
void Fl_BorderlessWindow::titleicon( HICON i )
{
    if ( convtitleiconimg != NULL )
    {
        releaseconvtitleiconimg();
    }

    convtitleiconimg = icon_to_flrgb( i );

    if ( convtitleiconimg != NULL )
    {
        titleicon( convtitleiconimg );
    }
}
#endif // _WIN32


int Fl_BorderlessWindow::handle( int e )
{
    int reti = Fl_Double_Window::handle( e );

    static int wsz_x = 0;
    static int wsz_y = 0;
    static int wsz_w = 0;
    static int wsz_h = 0;

    switch( e )
    {
        case FL_PUSH:
            {
                int cur_x = Fl::event_x();
                int cur_y = Fl::event_y();

                unsigned szflag = sizegriptest( cur_x, cur_y );

                if ( ( sizegripped == false ) && ( szflag > 0 ) )
                {
                    sizegrip_w = w();
                    sizegrip_h = h();

                    Fl::get_mouse( sizegrip_x, sizegrip_y );

                    sizegripped = true;
                }
            }
            break;

        case FL_RELEASE:
        case FL_LEAVE:
            if ( sizegripped == true )
            {
                sizegripped = false;

                sizegrip_flag = 0;
                sizegrip_x = 0;
                sizegrip_y = 0;
                sizegrip_w = 0;
                sizegrip_h = 0;

                regenbgcache( wsz_w, wsz_h );
                redraw();
            }
            break;

        case FL_MOVE:
            {
                int cur_x = Fl::event_x();
                int cur_y = Fl::event_y();

                sizegrip_flag = sizegriptest( cur_x, cur_y );

                switch( sizegrip_flag )
                {
                    case 1:
                        fl_cursor( FL_CURSOR_WE );
                        break;

                    case 2:
                        fl_cursor( FL_CURSOR_NS );
                        break;

                    case 3:
                        fl_cursor( FL_CURSOR_NWSE );
                        break;

                    default:
                        fl_cursor( FL_CURSOR_DEFAULT );
                        break;
                }

                if ( cbOnMove != NULL )
                {
                    cbOnMove( this, pdOnMove );
                }
            }
            break;

        case FL_DRAG:
            if ( sizegripped == true )
            {
                int new_x;
                int new_y;

                Fl::get_mouse( new_x, new_y );

                wsz_x = x();
                wsz_y = y();
                wsz_w = sizegrip_w + ( new_x - sizegrip_x );
                wsz_h = sizegrip_h + ( new_y - sizegrip_y );

                // Case WE, NS -> one side size be fixed.
                switch( sizegrip_flag )
                {
                    case 1:
                        wsz_h = sizegrip_h;
                        break;

                    case 2:
                        wsz_w = sizegrip_w;
                        break;
                }

                // check size range ...
                if ( ( min_h == max_h ) || ( wsz_h < min_h ) )
                {
                    wsz_h = min_h;
                }

                if ( ( min_w == max_w ) || ( wsz_w < min_w ) )
                {
                    wsz_w = min_w;
                }

                resize( wsz_x, wsz_y, wsz_w, wsz_h );
            }
            break;

#ifdef _WIN32
        // A Fake way for FLTK,
        case FL_SHOW:
            {
                HWND hWindow = fl_xid( this );
                if ( hWindow != NULL )
                {
                    ShowWindow( hWindow, SW_HIDE );

                    ulong style = 0;

                    // normal window style ...
                    style = GetWindowLong( hWindow, GWL_STYLE );
                    style |= CS_DROPSHADOW;

                    SetWindowLong( hWindow, GWL_STYLE, style );

                    // ExWindow style ...
                    style = GetWindowLong( hWindow, GWL_EXSTYLE );

                    style &= ~WS_EX_TOOLWINDOW;
                    //style |= WS_VISIBLE | WS_EX_LAYERED;
                    style |= WS_VISIBLE;

                    SetWindowLong( hWindow, GWL_EXSTYLE, style);

                    // Make black (0x00000000) becomes transparent ...
                    // SetLayeredWindowAttributes(hWindow, 0x00000000, 255, LWA_COLORKEY);

                    ShowWindow( hWindow, SW_SHOW );
                }
            }
            break;
#endif // _WIN32

    }

    return reti;
}

void Fl_BorderlessWindow::releaseconvtitleiconimg()
{
    if ( convtitleiconimg != NULL )
    {
        if ( ( convtitleiconimg->array != NULL ) && ( convtitleiconimg->alloc_array == 0 ) )
        {
            delete[] convtitleiconimg->array;
        }

        delete convtitleiconimg;

        convtitleiconimg = NULL;
    }
}

void Fl_BorderlessWindow::regenbgcache( int w, int h )
{
    if ( backgroundimg != NULL )
    {
        if ( backgroundimgcached != NULL )
        {
            fl_imgtk::discard_user_rgb_image( backgroundimgcached );
        }

        backgroundimgcached = fl_imgtk::rescale( (Fl_RGB_Image*)backgroundimg,
                                                 w, h,
                                                 fl_imgtk::BILINEAR );
    }
}

void Fl_BorderlessWindow::setsinglewindow( int stype )
{
    int cmax = 2;

    if ( stype > 0  )
    {
        cmax = 3;
    }

    singlewindowtype = stype;

    for( int cnt=0; cnt<cmax; cnt++ )
    {
        if( boxWindowButtons[cnt] != NULL )
        {
            boxWindowButtons[cnt]->deactivate();
            boxWindowButtons[cnt]->hide();
        }
    }

    if ( boxWindowTitle != NULL )
    {
        int new_w = boxWindowTitle->w() + ( DEF_FL_BORDERLESSWINDOW_ICON_BOX_WH * cmax );
        boxWindowTitle->resize( boxWindowTitle->x(),
                                boxWindowTitle->y(),
                                new_w,
                                boxWindowTitle->h() );
    }

#ifdef _WIN32
        // A Fake way for FLTK,
        HWND hWindow = fl_xid( this );
        if ( hWindow != NULL )
        {
            ShowWindow(hWindow, SW_HIDE);

            long style = GetWindowLong(hWindow, GWL_EXSTYLE);

            style &= ~WS_VISIBLE;
            style |= WS_EX_TOOLWINDOW;

            SetWindowLong(hWindow, GWL_EXSTYLE, style);
            ShowWindow( hWindow, SW_SHOW );
        }
#endif // _WIN32

    max_w = min_w;
    max_h = min_h;
}

void Fl_BorderlessWindow::setcleartype( bool b )
{
    if ( b == true )
    {
        this->box( FL_THIN_UP_FRAME );
        boxWindowIcon->box( FL_NO_BOX );
        boxWindowTitle->box( FL_NO_BOX );
        boxWindowButtons[0]->box( FL_NO_BOX );
        grpInnerWindow->box( FL_NO_BOX );
    }
    else
    {
        this->box( FL_FLAT_BOX );
        boxWindowIcon->box( defaultboxtype );
        boxWindowTitle->box( defaultboxtype );
        boxWindowButtons[0]->box( defaultboxtype );
        grpInnerWindow->box( defaultboxtype );
    }
}

void Fl_BorderlessWindow::color( Fl_Color c1, Fl_Color c2 )
{
    Fl_Double_Window::color( c1, c2 );

    if ( boxWindowIcon != NULL )
    {
        boxWindowIcon->color( c1, c2 );
    }

    if ( boxWindowTitle != NULL )
    {
        boxWindowTitle->color( c1, c2 );
    }

    for( int cnt=0; cnt<3; cnt++ )
    {
        if( boxWindowButtons[cnt] != NULL )
        {
            boxWindowButtons[cnt]->color( c1, c2 );
        }
    }
}

void Fl_BorderlessWindow::color( Fl_Color bg )
{
    color( bg, bg );
}

void Fl_BorderlessWindow::labelcolor( Fl_Color c )
{
    Fl_Double_Window::labelcolor( c );

    if ( boxWindowIcon != NULL )
    {
        boxWindowIcon->labelcolor( c );
    }

    if ( boxWindowTitle != NULL )
    {
        boxWindowTitle->labelcolor( c );
    }

    for( int cnt=0; cnt<3; cnt++ )
    {
        if( boxWindowButtons[cnt] != NULL )
        {
            boxWindowButtons[cnt]->labelcolor( c );
        }
    }
}

void Fl_BorderlessWindow::labelsize( Fl_Fontsize pix )
{
    Fl_Double_Window::labelsize( pix );

    if ( boxWindowIcon != NULL )
    {
        boxWindowIcon->labelsize( pix );
    }

    if ( boxWindowTitle != NULL )
    {
        boxWindowTitle->labelsize( pix );
    }

    for( int cnt=0; cnt<3; cnt++ )
    {
        if( boxWindowButtons[cnt] != NULL )
        {
            boxWindowButtons[cnt]->labelsize( pix );
        }
    }
}

void Fl_BorderlessWindow::enlarge()
{
    if ( maximized_wh == false )
    {
        if ( boxWindowButtons[1] != NULL )
        {
            boxWindowButtons[1]->do_callback();
        }
    }
}

void Fl_BorderlessWindow::WCB( Fl_Widget* w )
{
    if ( w == boxWindowButtons[0] )
    {
        iconize();
    }

    if ( w == boxWindowButtons[1] )
    {
        if ( maximized_wh == true )
        {
            if( boxWindowButtons[1] != NULL )
            {
                boxWindowButtons[1]->label( DEF_FL_BWINDOW_SYS_BTN_TEXT_MAXIMIZE );
            }

            regenbgcache( prev_fs_w, prev_fs_h );
            resize( prev_fs_x, prev_fs_y, prev_fs_w, prev_fs_h );

            maximized_wh = false;
        }
        else
        {
            prev_fs_x = x();
            prev_fs_y = y();
            prev_fs_w = this->w();
            prev_fs_h = h();

            if( boxWindowButtons[1] != NULL )
            {
                boxWindowButtons[1]->label( DEF_FL_BWINDOW_SYS_BTN_TEXT_RETURNSIZE );
            }

            int scrn_x;
            int scrn_y;
            int scrn_w;
            int scrn_h;

            Fl::screen_work_area( scrn_x, scrn_y, scrn_w, scrn_h );

            if ( ( max_w > 0 ) && ( max_w == min_w ) )
            {
                scrn_x = x();
                scrn_w = max_w;
            }

            if ( ( max_h > 0 ) && ( max_h == min_h ) )
            {
                scrn_y = y();
                scrn_h = max_h;
            }

            regenbgcache( scrn_w, scrn_h );
            resize( scrn_x, scrn_y, scrn_w, scrn_h );

            maximized_wh = true;
        }
    }

    if ( w == boxWindowButtons[2] )
    {
        //hide();
        do_callback();
        return;
    }
}

int Fl_BorderlessWindow::clientarea_x()
{
    if ( grpInnerWindow != NULL )
    {
        return grpInnerWindow->x();
    }

    return 0;
}

int Fl_BorderlessWindow::clientarea_y()
{
    if ( grpInnerWindow != NULL )
    {
        return grpInnerWindow->y();
    }

    return 0;
}

int Fl_BorderlessWindow::clientarea_w()
{
    if ( grpInnerWindow != NULL )
    {
        return grpInnerWindow->w();
    }

    return w();
}

int Fl_BorderlessWindow::clientarea_h()
{
    if ( grpInnerWindow != NULL )
    {
        return grpInnerWindow->h();
    }

    return h();
}

Fl_Group* Fl_BorderlessWindow::clientarea()
{
    if ( grpInnerWindow != NULL )
    {
        return grpInnerWindow;
    }

    return this;
}

////////////////////////////////////////////////////////////////////////////////

void Fl_BorderlessWindow::Fl_BorderlessWindow_CB( Fl_Widget* w, void* p )
{
    if ( p != NULL )
    {
        Fl_BorderlessWindow* pw = (Fl_BorderlessWindow*)p;
        pw->WCB( w );
    }
}