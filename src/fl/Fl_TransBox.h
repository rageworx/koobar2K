#ifndef __FL_TRANSBOX_H__
#define __FL_TRANSBOX_H__

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Image.H>

class Fl_TransBox : public Fl_Box
{
    public:
        Fl_TransBox(int x,int y,int w,int h, char* l = 0);
        virtual ~Fl_TransBox();

    public:
        void color(Fl_Color c);
        void set_alpha(unsigned char a);
        void set_dragEnabled(bool enabled) { dragEnabled = enabled; }
        void set_displayimage(Fl_Image* aimg) { dispimg = aimg; }

    protected:
        int handle(int e);
        void draw();
        void resize( int x, int y, int w, int h );

    private:
        void fill_buffer();

    private:
        unsigned char*  buffer;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char alpha;
        Fl_RGB_Image* img;
        bool          dragEnabled;
        Fl_Image*     dispimg;
        bool          imgdisabled;

};

#endif /// __FL_TRANSBOX_H__
