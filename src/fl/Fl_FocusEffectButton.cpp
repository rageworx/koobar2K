#include "Fl_FocusEffectButton.H"

#include <FL/fl_draw.H>
#include "fl_imgtk.h"

////////////////////////////////////////////////////////////////////////////////

#define FL_FOCUSEFFECTBUTTON_ALPHA_MIN  0.5f
#define FL_FOCUSEFFECTBUTTON_ALPHA_MAX  1.0f
#define FL_FOCUSEFFECTBUTTON_ALPHA_TRM  0.1f
#define FL_FOCUSEFFECTBUTTON_ALPHA_TMR  0.1f

////////////////////////////////////////////////////////////////////////////////

void fl_focuseffectbutton_timer_cb( void* p );

////////////////////////////////////////////////////////////////////////////////

Fl_FocusEffectButton::Fl_FocusEffectButton( int X, int Y, int W, int H, const char* L )
 : Fl_Button( X, Y, W, H, L ),
   _imglocked( false ),
   _btnimg( NULL ),
   _cachedimg( NULL ),
   _focusstate( 0 ),
   _needchange( false ),
   _alpha( FL_FOCUSEFFECTBUTTON_ALPHA_MIN ),
   _drawparent( NULL )
{
    Fl_Button::clear_visible_focus();
    Fl_Button::box( FL_NO_BOX );
    Fl_Button::align( FL_ALIGN_INSIDE | FL_ALIGN_CENTER );

    Fl::add_timeout( FL_FOCUSEFFECTBUTTON_ALPHA_TMR,
                     fl_focuseffectbutton_timer_cb,
                     this );
}

Fl_FocusEffectButton::~Fl_FocusEffectButton()
{
    Fl::remove_timeout( fl_focuseffectbutton_timer_cb );

    deimage();
}

void Fl_FocusEffectButton::deimage()
{
    _imglocked = true;

    fl_imgtk::discard_user_rgb_image( _cachedimg );
    fl_imgtk::discard_user_rgb_image( _btnimg );

    _imglocked = false;
}

void Fl_FocusEffectButton::image( Fl_RGB_Image* img )
{
    _imglocked = true;

    if ( _btnimg != NULL )
    {
        fl_imgtk::discard_user_rgb_image( _btnimg );
    }

    if ( img != NULL )
    {
        _btnimg = (Fl_RGB_Image*)img->copy();

        if ( _focusstate == 1 )
        {
            _alpha = FL_FOCUSEFFECTBUTTON_ALPHA_MAX;
        }
        else
        {
            _alpha = FL_FOCUSEFFECTBUTTON_ALPHA_MIN;
        }

        _imglocked = false;

        regencachedimage();
    }

    _imglocked = false;
}

void Fl_FocusEffectButton::tc()
{
    switch( _focusstate )
    {
        case 0: /// decrease alpha;
            if ( _alpha > FL_FOCUSEFFECTBUTTON_ALPHA_MIN )
            {
                _alpha -= FL_FOCUSEFFECTBUTTON_ALPHA_TRM;
                _needchange = true;
            }
            else
            {
                _needchange = false;
            }
            break;

        case 1: /// increase alpha;
            if ( _alpha < FL_FOCUSEFFECTBUTTON_ALPHA_MAX )
            {
                _alpha += FL_FOCUSEFFECTBUTTON_ALPHA_TRM;
                _needchange = true;
            }
            else
            {
                _needchange = false;
            }
            break;
    }

    if ( _needchange == true )
    {
        // recreate cached image.
        regencachedimage();
        //damage(1);

        if ( _drawparent != NULL )
        {
            _drawparent->redraw();
        }

        Fl::repeat_timeout( FL_FOCUSEFFECTBUTTON_ALPHA_TMR,
                            fl_focuseffectbutton_timer_cb,
                            this );
    }
}

void Fl_FocusEffectButton::regencachedimage()
{
    if ( _imglocked == true )
        return;

    _imglocked = true;

    if ( _btnimg != NULL )
    {
        fl_imgtk::discard_user_rgb_image( _cachedimg );

        _cachedimg = fl_imgtk::applyalpha( _btnimg, _alpha );
    }

    _imglocked = false;
}

int Fl_FocusEffectButton::handle( int e )
{
    bool changed = false;

    switch( e )
    {
        case FL_ENTER:
            if ( _focusstate != 1 )
            {
                _focusstate = 1;
                changed = true;
            }
            break;

        case FL_LEAVE:
            if ( _focusstate != 0 )
            {
                _focusstate = 0;
                changed = true;
            }
            break;
    }

    if ( changed == true )
    {
        tc();
    }

    return Fl_Button::handle( e );
}

void Fl_FocusEffectButton::draw()
{
    if ( _cachedimg == NULL )
    {
        Fl_Button::draw();
        return;
    }

    if ( _imglocked == false )
    {
        int px = x() + ( w() - _cachedimg->w() ) / 2;
        int py = y() + ( h() - _cachedimg->h() ) / 2;

        _cachedimg->draw( px, py );

        if ( _drawparent != NULL )
        {
            if ( _drawparent )
            {
                _drawparent->damage( 1 );
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void fl_focuseffectbutton_timer_cb( void* p )
{
    if ( p != NULL )
    {
        Fl_FocusEffectButton* pfeb = (Fl_FocusEffectButton*)p;
        pfeb->tc();
    }
}
