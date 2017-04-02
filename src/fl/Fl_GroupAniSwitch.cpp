#include "Fl_GroupAniSwitch.h"

#include <FL/Fl_Image_Surface.H>
#include <FL/fl_draw.H>

#include "fl_imgtk.h"

#ifdef DEBUG
#include "png/png.h"
#endif /// DEBUG

#define DRAWTIMER_MS        10
#define MINIMUM_DRAW_MS     0.1f

////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
bool savetocolorpng( Fl_RGB_Image* src, const wchar_t* fpath );
#endif /// DEBUG

static void  fl_gas_timer_cb( void* p );

////////////////////////////////////////////////////////////////////////////////

Fl_GroupAniSwitch::Fl_GroupAniSwitch( Fl_Group* host,
                                      Fl_Group* src, Fl_Group* dst,
                                      AnimationType anitype,
                                      bool autoHide, unsigned ms )
 : ani_type( anitype),
   grp_host( host ),
   grp_src( src ),
   grp_dst( dst ),
   bg_blurred_img( NULL ),
   draw_ms( MINIMUM_DRAW_MS ),
   draw_bg( false ),
   anim_null( false ),
   anim_finished( false ),
   auto_hide( autoHide ),
   max_movel( 0 )
{
    // checks variables ...
    if ( ( grp_host == NULL ) || ( grp_src == NULL ) || ( grp_dst == NULL ) )
    {
        anim_null = true;
        anim_finished = true;
    }
    else
    {
        switch( ani_type )
        {
            case ATYPE_RIGHT2LEFT:
            case ATYPE_JUSTHOW:
                bg_blurred_img = fl_imgtk::draw_widgetimage( src );
                break;

            case ATYPE_LEFT2RIGHT:
                bg_blurred_img = fl_imgtk::draw_widgetimage( dst );
                break;
        }

#ifdef DEBUG
		// you can unblock below line to check your blurred image works or not.
        //savetocolorpng( bg_blurred_img, L"BLURRED.PNG" );
#endif // DEBUG
    }

    // remember original positions ..
    src_org_pt.x = src->x();
    src_org_pt.y = src->y();
    dst_org_pt.x = dst->x();
    dst_org_pt.y = dst->y();

    // Calculate drawing ...
    max_movel = src->w() / 100;

    int dst_w = src->w();
    int lps = 0;

    while( dst_w > 0 )
    {
        int movx = dst_w / 2;
        if ( movx > max_movel )
            movx = max_movel;

        dst_w -= max_movel;
        lps++;

        if ( dst_w <= 1 )
            dst_w = 0;
    }

    if ( lps > 0 )
    {
        draw_ms = float( ms / lps ) / 1000.0f;
    }

    switch( ani_type )
    {
        case ATYPE_RIGHT2LEFT:
            src_src_pt.x = src->x();
            src_src_pt.y = src->y();
            src_dst_pt.x = src->x() - src->w();
            src_dst_pt.y = src->y();

            dst_src_pt.x = dst->x() + src->w();
            dst_src_pt.y = dst->y();
            dst_dst_pt.x = src->x();
            dst_dst_pt.y = src->y();
            break;

        case ATYPE_LEFT2RIGHT:
            src_src_pt.x = src->x() - src->w();
            src_src_pt.y = src->y();
            src_dst_pt.x = src->x();
            src_dst_pt.y = src->y();

            dst_src_pt.x = src->x();
            dst_src_pt.y = dst->y();
            dst_dst_pt.x = src->x() + src->w();
            dst_dst_pt.y = src->y();
            break;

        case ATYPE_JUSTHOW:
            src_src_pt.x = src->x();
            src_src_pt.y = src->y();
            src_dst_pt.x = src->x();
            src_dst_pt.y = src->y();

            dst_src_pt.x = dst->x();
            dst_src_pt.y = dst->y();
            dst_dst_pt.x = src->x();
            dst_dst_pt.y = src->y();
            break;
    }

    // Temporary insert background group...
    grp_tmp_bg = new Fl_Group( grp_src->x(), grp_src->y(), grp_src->w(), grp_src->h() );
    if ( grp_tmp_bg != NULL )
    {
        grp_tmp_bg->begin();

        box_tmp_bg = new Fl_Box( grp_src->x(), grp_src->y(), grp_src->w(), grp_src->h() );
        if ( box_tmp_bg != NULL )
        {
            box_tmp_bg->box( FL_FLAT_BOX );
            box_tmp_bg->image( bg_blurred_img );
        }

        grp_tmp_bg->end();

        grp_host->insert( *grp_tmp_bg, grp_src );
    }

    if ( grp_dst->visible_r() == 0 )
    {
        grp_dst->show();
    }

    Fl::add_timeout( draw_ms, fl_gas_timer_cb, this );
}

Fl_GroupAniSwitch::~Fl_GroupAniSwitch()
{
    if ( grp_host != NULL )
    {
        grp_host->remove( grp_tmp_bg );
    }

    if ( box_tmp_bg != NULL )
    {
        delete box_tmp_bg;
    }

    if ( grp_tmp_bg != NULL )
    {
        delete grp_tmp_bg;
    }

    discard_rgb_image( bg_blurred_img );
}

bool Fl_GroupAniSwitch::generate_img( Fl_Group* src, Fl_RGB_Image* &dst )
{
    if ( src != NULL )
    {
        Fl_Image_Surface* imgsurf = new Fl_Image_Surface( src->w(), src->h(), 0 );

        if ( imgsurf != NULL )
        {
            imgsurf->set_current();
            imgsurf->draw( src );
            dst = imgsurf->image();

            Fl_Display_Device::display_device()->set_current();

            delete imgsurf;

            if ( dst != NULL )
                return true;
        }
    }

    return false;
}


void Fl_GroupAniSwitch::NextStep()
{
    bool neednext = false;

    if ( src_src_pt.x != src_dst_pt.x )
    {
        int moves = 0;

        if ( src_src_pt.x > src_dst_pt.x )
        {
            moves = calc_moves( src_src_pt.x, src_dst_pt.x );
            src_src_pt.x -= moves;
        }
        else
        if ( src_src_pt.x < src_dst_pt.x )
        {
            moves = calc_moves( src_dst_pt.x, src_src_pt.x );
            src_src_pt.x += moves;
        }

        if ( moves != 0 )
            neednext = true;
    }

    if ( dst_src_pt.x != dst_dst_pt.x )
    {
        int moves = 0;

        if ( dst_src_pt.x > dst_dst_pt.x )
        {
            moves = calc_moves( dst_src_pt.x, dst_dst_pt.x );
            dst_src_pt.x -= moves;
        }
        else
        if ( dst_src_pt.x < dst_dst_pt.x )
        {
            moves = calc_moves( dst_dst_pt.x, dst_src_pt.x );
            dst_src_pt.x += moves;
        }

        if ( moves != 0 )
            neednext = true;

    }

    grp_src->position( src_src_pt.x, src_src_pt.y );
    grp_dst->position( dst_src_pt.x, dst_src_pt.y );

    grp_host->redraw();

    if ( neednext == true )
    {
        Fl::repeat_timeout( draw_ms, fl_gas_timer_cb, this );
    }
    else
        Finish();
}

void Fl_GroupAniSwitch::Finish()
{
    Fl::remove_timeout( fl_gas_timer_cb, this );

    grp_src->position( src_org_pt.x, src_org_pt.y );
    grp_dst->position( dst_org_pt.x, dst_org_pt.y );

    grp_host->redraw();

    if ( auto_hide == true )
    {
        switch( ani_type )
        {
            case ATYPE_LEFT2RIGHT:
                grp_dst->hide();
                break;

            case ATYPE_RIGHT2LEFT:
                grp_src->hide();
                break;
        }
    }

    grp_host->redraw();

    anim_finished = true;
}

void Fl_GroupAniSwitch::WaitForFinish()
{
    while( anim_finished == false )
    {
        Fl::wait( 10 );
    }
}

int Fl_GroupAniSwitch::calc_moves( int v1, int v2 )
{
    int moves = 0;

    if ( v1 > v2 )
    {
        moves = ( v1 - v2 ) / 2;

        if ( moves <= 1 )
            moves = 0;
    }
    else
    if ( v1 < v2 )
    {
        moves = ( v2 - v1 ) / 2;

        if ( moves <= 1 )
            moves = 0;
    }

    return moves;
}

void Fl_GroupAniSwitch::discard_rgb_image( Fl_RGB_Image* &img )
{
    if( img != NULL )
    {
        if ( ( img->array != NULL ) && ( img->alloc_array == 0 ) )
        {
            delete[] img->array;
        }

        delete img;
        img = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

static void  fl_gas_timer_cb( void* p )
{
    if ( p != NULL )
    {
        Fl_GroupAniSwitch* fgas = (Fl_GroupAniSwitch*)p;
        fgas->NextStep();
    }
}

#ifdef DEBUG
bool savetocolorpng( Fl_RGB_Image* src, const wchar_t* fpath )
{
    if ( src == NULL )
        return false;

    if ( src->d() < 3 )
        return false;

    FILE* fp = _wfopen( fpath, L"wb" );

    if ( fp == NULL )
        return false;

    png_structp png_ptr     = NULL;
    png_infop   info_ptr    = NULL;
    png_bytep   row         = NULL;

    png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if ( png_ptr != NULL )
    {
        info_ptr = png_create_info_struct( png_ptr );
        if ( info_ptr != NULL )
        {
            if ( setjmp( png_jmpbuf( (png_ptr) ) ) == 0 )
            {
                int mx = src->w();
                int my = src->h();
                int pd = 3;

                png_init_io( png_ptr, fp );
                png_set_IHDR( png_ptr,
                              info_ptr,
                              mx,
                              my,
                              8,
                              PNG_COLOR_TYPE_RGB,
                              PNG_INTERLACE_NONE,
                              PNG_COMPRESSION_TYPE_BASE,
                              PNG_FILTER_TYPE_BASE);

                png_write_info( png_ptr, info_ptr );

                row = (png_bytep)malloc( src->w() * sizeof( png_byte ) * 3 );
                if ( row != NULL )
                {
                    const char* buf = src->data()[0];
                    int bque = 0;

                    for( int y=0; y<my; y++ )
                    {
                        for( int x=0; x<mx; x++ )
                        {
                            row[ (x*3) + 0 ] = buf[ bque + 0 ];
                            row[ (x*3) + 1 ] = buf[ bque + 1 ];
                            row[ (x*3) + 2 ] = buf[ bque + 2 ];
                            bque += pd;
                        }

                        png_write_row( png_ptr, row );
                    }

                    png_write_end( png_ptr, NULL );

                    fclose( fp );

                    free(row);
                }

                png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
                png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

                return true;
            }
        }
    }
}
#endif /// of DEBUG
