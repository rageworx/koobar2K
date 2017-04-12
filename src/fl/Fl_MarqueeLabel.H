#ifndef __FL_MARQUEELABEL_H__
#define __FL_MARQUEELABEL_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Box.H>

class Fl_MarqueeLabel : public Fl_Box
{
    public:
        Fl_MarqueeLabel( int x, int y, int w, int h, const char* l = 0 );
        ~Fl_MarqueeLabel();

    public: /// overrides --
        void label( const char* l = 0 );
        void draw();
        void resize( int x, int y, int w, int h );

    public:
        void timercb();
        void stopupdate()       { updating = false; }
        void continueupdate()   { updating = true; }

    public:
        void checkcondition();
        void resettimer( bool force = false );

    protected:
        bool    needmarquee;
        int     marqueedirection;
        int     marquee_l;
        int     marquee_d;
        float   marquee_interval;
        int     put_y;
        int     put_w;
        int     put_h;
        bool    updating;
        bool    timered;
};

#endif /// of __FL_MARQUEELABEL_H__
