#ifndef __FL_SEEKBAR_H__
#define __FL_SEEKBAR_H__

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_RGB_Image.H>

class Fl_SeekBar : public Fl_Box
{
    public:
        Fl_SeekBar( int X, int Y, int W, int H );
        ~Fl_SeekBar();

    public:
        void range( unsigned minv, unsigned maxv );
        void bufferedposition( unsigned pos );
        void currentposition( unsigned pos );
        void update();

    protected:
        void createImage();
        void updateImage();
        void deleteImage();

    protected:
        void draw();

    protected:
        unsigned char*  pixels;
        Fl_RGB_Image*   srcimage;
        Fl_RGB_Image*   cachedimage;
        unsigned        range_min;
        unsigned        range_max;
        unsigned        pos_cur;
        unsigned        pos_buffered;
};

#endif // __FL_SEEKBAR_H__
