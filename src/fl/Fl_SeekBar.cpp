#include "Fl_SeekBar.h"

#include <Fl/fl_draw.H>
#include "fl_imgtk.h"

Fl_SeekBar::Fl_SeekBar( int X, int Y, int W, int H )
 : Fl_Box( X, Y, W, H ),
   pixels( NULL ),
   range_min( 0 ),
   range_max( 9999999 ),
   pos_cur( 0 ),
   pos_buffered( 0 ),
   srcimage( NULL ),
   cachedimage( NULL )
{
    createImage();
    updateImage();
}

Fl_SeekBar::~Fl_SeekBar()
{
    deleteImage();
}

void Fl_SeekBar::range( unsigned minv, unsigned maxv )
{
    range_min = minv;
    range_max = maxv;
}

void Fl_SeekBar::bufferedposition( unsigned pos )
{
    pos_buffered = pos;

    if ( pos_buffered > range_max )
        pos_buffered = range_max;
}

void Fl_SeekBar::currentposition( unsigned pos )
{
    pos_cur = pos;

    if ( pos_cur > range_max )
        pos_cur = range_max;
}

void Fl_SeekBar::update()
{
    updateImage();
}

void Fl_SeekBar::createImage()
{
    if ( pixels != NULL )
    {
        delete srcimage;
        delete[] pixels;
        pixels = NULL;
        srcimage = NULL;
    }

    unsigned psz = w() * h() * 4;
    pixels = new unsigned char [ psz ];

    if ( pixels != NULL )
    {
        memset( pixels, 0, psz );

        srcimage = new Fl_RGB_Image( pixels, w(), h(), 4 );
    }
}

void Fl_SeekBar::deleteImage()
{
    if ( pixels != NULL )
    {
        delete srcimage;
        delete[] pixels;
        pixels = NULL;
        srcimage = NULL;
    }

    fl_imgtk::discard_user_rgb_image( cachedimage );
}

void Fl_SeekBar::updateImage()
{
    if ( srcimage != NULL )
    {
        uchar* refp = (uchar*)srcimage->data()[0];
        unsigned pos_cmax = range_max - range_min;

        // Clear
        unsigned psz = w() * h() * 4;
        memset( refp, 0, psz );

        // Let's draw something here.

        int lposmin = 0;
        int lposmax = w();
        int lposy   = h() / 2;

        // Draw a base line.
        for( unsigned cnt=lposmin; cnt<lposmax; cnt++ )
        {
            uchar* pptr = &refp[ ( w() * lposy + cnt ) * 4 ];
            memset( pptr, 0xFF, 3 );
            pptr[ 3 ] = 0x4F;
        }

        int lposw = w() - ( lposmin * 2 );

        // Draw a buffered length
        if ( pos_buffered > 1 )
        {
            unsigned lposbuffered = ( (float)pos_buffered / (float)pos_cmax ) * (float)lposw;

            if ( lposbuffered < w() )
            {
                for( unsigned cnt=lposmin; cnt<lposbuffered; cnt++ )
                {
                    uchar* pptr = &refp[ ( w() * lposy + cnt ) * 4 ];
                    pptr[ 3 ] = 0x8F;
                }
            }
        }

        // Draw a knob.
        int knobw = (float)w() * 0.1f;
        int knobpos = 0;
        if ( pos_cur > 1 )
        {
            knobpos = ( (float)pos_cur / (float)pos_cmax ) * (float)( lposw - knobw );
        }
        knobpos += lposmin;
        lposw = knobpos + knobw;

        if ( lposw > w() )
            lposw = w();

        if ( lposy > 0 )
        {
            for( unsigned cnt=knobpos; cnt<lposw; cnt++ )
            {
                uchar* pptr = &refp[ ( w() * ( lposy - 1 ) + cnt ) * 4 ];
                pptr[ 0 ] = 0xFF;
                pptr[ 1 ] = 0x55;
                pptr[ 2 ] = 0x55;
                pptr[ 3 ] = 0x4F;

                pptr = &refp[ ( w() * ( lposy + 1 ) + cnt ) * 4 ];
                pptr[ 0 ] = 0xFF;
                pptr[ 1 ] = 0x55;
                pptr[ 2 ] = 0x55;
                pptr[ 3 ] = 0x4F;

                pptr = &refp[ ( w() * lposy + cnt ) * 4 ];
                pptr[ 0 ] = 0xFF;
                pptr[ 1 ] = 0x55;
                pptr[ 2 ] = 0x55;
                pptr[ 3 ] = 0xAF;
            }
        }

        srcimage->uncache();

        if ( cachedimage != NULL )
        {
            fl_imgtk::discard_user_rgb_image( cachedimage );
        }

        cachedimage = (Fl_RGB_Image*)srcimage->copy();
    }
}

void Fl_SeekBar::draw()
{
    fl_push_clip( x(), y(), w(), h() );

    if ( cachedimage != NULL )
    {
        cachedimage->draw( x(), y() );
    }

    fl_pop_clip();
}
