#include "themes.h"

////////////////////////////////////////////////////////////////////////////////

const int default_font_size     = 12;
const Fl_Color color_bg_base    = 0x99999900;
const Fl_Color color_bg_group   = 0x99999900;
const Fl_Color color_fg_base    = 0x55555500;
const Fl_Color color_fg_menu    = 0x33333300;

////////////////////////////////////////////////////////////////////////////////

void rkrawv::InitTheme()
{
    // Something do.
}

void rkrawv::ApplyBWindowTheme( Fl_BorderlessWindow* w )
{
    if ( w != NULL )
    {
        w->color( color_bg_base );
        w->labelcolor( color_fg_base );
        w->labelsize( default_font_size );
    }
}

void rkrawv::ApplyDefaultTheme( Fl_Widget* w )
{
    if ( w != NULL )
    {
        w->color( color_bg_base );
        w->labelcolor( color_fg_base );
        w->labelsize( default_font_size );
    }
}

void rkrawv::ApplyButtonTheme( Fl_Widget* w )
{
    if ( w != NULL )
    {
        w->box( FL_THIN_UP_BOX );
        ApplyDefaultTheme( w );
    }
}

void rkrawv::ApplyGroupTheme( Fl_Group* w )
{
    if ( w != NULL )
    {
        ApplyDefaultTheme( w );
        w->color( color_bg_group );
    }
}

void rkrawv::ApplyScrollTheme( Fl_Scroll* w )
{
    if ( w != NULL )
    {
        ApplyDefaultTheme( w );
        w->color( color_bg_group );
        w->scrollbar_size( 5 );
    }
}

void rkrawv::ApplyMenuBarTheme( Fl_Menu_Bar* w )
{
    if ( w != NULL )
    {
        ApplyDefaultTheme( w );
        w->box( FL_FLAT_BOX );
        w->textcolor( color_fg_menu );
        w->textsize( default_font_size );
    }
}

void rkrawv::ApplyStatusBoxTheme( Fl_Box* w )
{
    if ( w != NULL )
    {
        ApplyDefaultTheme( w );
        w->align( FL_ALIGN_INSIDE | FL_ALIGN_LEFT );
        w->box( FL_FLAT_BOX );
    }
}
