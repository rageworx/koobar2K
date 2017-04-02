#ifndef __THEMES_H__
#define __THEMES_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Menu_Bar.H>
#include "Fl_BorderlessWindow.H"

namespace rkrawv
{
    void InitTheme();
    void ApplyBWindowTheme( Fl_BorderlessWindow* w );
    void ApplyDefaultTheme( Fl_Widget* w );
    void ApplyButtonTheme( Fl_Widget* w );
    void ApplyGroupTheme( Fl_Group* w );
    void ApplyScrollTheme( Fl_Scroll* w );
    void ApplyMenuBarTheme( Fl_Menu_Bar* w );
    void ApplyStatusBoxTheme( Fl_Box* w );
};

#endif /// of __THEMES_H__
