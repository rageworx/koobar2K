#include "Fl_MarqueeLabel.H"
#include <FL/fl_draw.H>

////////////////////////////////////////////////////////////////////////////////

static int  fl_marqueelabel_timermxt = 0;
static void fl_marqueelabel_timercb( void* p );

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Fl_MarqueeLabel::Fl_MarqueeLabel( int x, int y, int w, int h, const char* l )
 : Fl_Box( x, y, w, h, l ),
   needmarquee( false ),
   marqueedirection( -1 ),
   marquee_l( 0 ),
   marquee_d( 0 ),
   marquee_interval( 0.1f ),
   put_y( 0 ),
   put_w( 0 ),
   put_h( 0 ),
   updating( true ),
   timered( false ),
   drawbyme( true )
{
    Fl_Box::box( FL_NO_BOX );
    Fl_Box::align( FL_ALIGN_INSIDE | FL_ALIGN_LEFT );

    Fl::add_timeout( marquee_interval, fl_marqueelabel_timercb, this );
}

Fl_MarqueeLabel::~Fl_MarqueeLabel()
{
    Fl::remove_timeout( fl_marqueelabel_timercb, this );
}

void Fl_MarqueeLabel::draw()
{
    if ( active_r() == 0 )
        return;

    fl_push_clip( x(), y(), w(), h() );

    fl_color( labelcolor() );
    fl_font( labelfont(), labelsize() );

    const char* l = Fl_Box::label();

    if ( needmarquee == true )
    {
        fl_draw( l, x() + marquee_l,  y() + put_y );
    }
    else
    {
        int px = ( w() - put_w ) / 2;

        fl_draw( l, x() + px, y() + put_y );
    }

    fl_pop_clip();
}

void Fl_MarqueeLabel::label( const char* l )
{
    Fl_Box::label( l );

    needmarquee = false;
    marquee_d = -1;
    marquee_l = -1;
    marqueedirection = -1;

    resettimer();
}

void Fl_MarqueeLabel::resize( int x, int y, int w, int h )
{
    Fl_Box::resize( x, y, w, h );

    checkcondition();
}

void Fl_MarqueeLabel::timercb()
{
    if ( timered == false )
    {
        resettimer( true );
    }

    if ( needmarquee == true )
    {
        switch( marqueedirection )
        {
            case 0 : /// <---
                marquee_l -= 1;
                if ( marquee_l <= -marquee_d )
                {
                    marqueedirection = 1;
                }
                break;

            case 1 : /// --->
                marquee_l += 1;
                if ( marquee_l >= 0 )
                {
                    marqueedirection = 0;
                }
                break;
        }

        if ( ( updating == true ) && ( drawbyme == true ) )
        {
            redraw();
        }

        Fl::repeat_timeout( marquee_interval, fl_marqueelabel_timercb, this );
    }
}

void Fl_MarqueeLabel::checkcondition()
{
    put_w = 0;
    put_h = 0;

    const char* l = Fl_Box::label();

    if ( l == NULL )
        return;

    fl_font( labelfont(), labelsize() );
    fl_measure( l, put_w, put_h );

    //printf("measured : %s -> %d x %d , h() = %d\n", label(), put_w, put_h, h() );

    put_y = ( h() / 2 ) + ( put_h / 3 );

    if ( put_w > w() )
    {
        marquee_d = put_w- w();
        if ( needmarquee == false )
        {
            needmarquee = true;

            switch( marqueedirection )
            {
                case -1:
                    marqueedirection = 0;
                case 0:
                    marquee_l = 0;
                    break;

                case 1:
                    marquee_l = -marquee_d;
                    break;
            }

            if ( timered == false )
            {
                timered = true;
                Fl::repeat_timeout( marquee_interval, fl_marqueelabel_timercb, this );
            }
        }
    }
    else
    {
        needmarquee = false;
        marquee_d = -1;
        marquee_l = -1;
        marqueedirection = -1;

        resettimer();

        if ( ( updating == true ) && ( drawbyme == true ) )
        {
            redraw();
        }
    }
}

void Fl_MarqueeLabel::resettimer( bool force )
{
    if ( ( timered == true ) || ( force == true ) )
    {
        timered = false;
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void fl_marqueelabel_timercb( void* p )
{
    if ( p != NULL )
    {
        if ( fl_marqueelabel_timermxt > 0  )
            return;

        fl_marqueelabel_timermxt ++;
        Fl_MarqueeLabel* pfml = (Fl_MarqueeLabel*)p;
        pfml->timercb();
        fl_marqueelabel_timermxt --;
    }
}
