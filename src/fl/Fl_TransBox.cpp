#include "Fl_TransBox.h"


Fl_TransBox::Fl_TransBox(int x, int y, int w, int h, char* l)
 : Fl_Box(x, y, w, h) ,
   alpha(0x80),
   dragEnabled(false),
   dispimg(NULL)
 {
    box(FL_NO_BOX);
    buffer = new unsigned char[4*w*h];
    img = new Fl_RGB_Image(buffer, w, h, 4);
    color((Fl_Color)0);
}

Fl_TransBox::~Fl_TransBox()
{
    deimage();

    if ( buffer != NULL )
    {
        delete[] buffer;
    }
}

void Fl_TransBox::color(Fl_Color c)
{
    r = (c >> 24);
    g = (c >> 16);
    b = (c >> 8);

    fill_buffer();
    img->uncache();
}

void Fl_TransBox::set_alpha(unsigned char a)
{
    alpha = a;
    fill_buffer();
    img->uncache();
}

int Fl_TransBox::handle(int e)
{
    // allows to drag the TransparentBox

    static int xn;
    static int yn;

    switch( e )
    {
        case FL_PUSH:
            xn = Fl::event_x() - x();
            yn = Fl::event_y() - y();
            return 1;

        case FL_DRAG:
            if ( dragEnabled == true )
            {
                position(Fl::event_x() - xn, Fl::event_y() - yn);
                window()->redraw();
            }
            return 1;

        case FL_RELEASE:
            return 1;

        default:
            return 0;
    }
}

void Fl_TransBox::fill_buffer()
{
    unsigned char *p = buffer;

    for (int i = 0; i < 4*w()*h(); i+=4)
    {
        *p++ = r;
        *p++ = g;
        *p++ = b;
        *p++ = alpha;
    }
}

void Fl_TransBox::draw()
{
    if ( img != NULL )
    {
        img->draw( x(), y() );
    }

    if ( label() != NULL )
    {
        Fl_Box::draw_label();
    }
    else
    if ( dispimg != NULL )
    {
        int putX = ( w() - dispimg->w() ) / 2;
        int putY = ( h() - dispimg->h() ) / 2;

        dispimg->draw(x() + putX, y() + putY);

    }
}
