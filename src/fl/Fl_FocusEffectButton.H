#ifndef __FL_FOCUSEFFECTBUTTON_H__
#define __FL_FOCUSEFFECTBUTTON_H__

#include <FL/Fl.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>

class Fl_FocusEffectButton : public Fl_Button
{
    public:
        Fl_FocusEffectButton( int X, int Y, int W, int H, const char* L=0 );
        ~Fl_FocusEffectButton();

    public:
        void drawparent( Fl_Group* pW )    { _drawparent = pW; }
        Fl_Widget* drawparent()             { return _drawparent; }
        void deimage();
        void image( Fl_RGB_Image* img );
        Fl_RGB_Image* image()               { return _btnimg; }

    public:
        void tc();  /// timer call , must be called by timer.

    protected:
        void regencachedimage();

    protected:
        int handle( int e );
        void draw();

    protected:
        bool            _imglocked;
        int             _focusstate;
        bool            _needchange;
        float           _alpha;

    protected:
        Fl_RGB_Image*   _btnimg;
        Fl_RGB_Image*   _cachedimg;
        Fl_Group*       _drawparent;
};

#endif /// of __FL_FOCUSEFFECTBUTTON_H__
