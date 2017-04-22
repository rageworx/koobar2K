#ifndef __FL_NOBACKSCROLL_H__
#define __FL_NOBACKSCROLL_H__

#include <FL/Fl.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Group.H>

class Fl_NobackScroll : public Fl_Group
{
    public:
        Fl_NobackScroll(int X,int Y,int W,int H,const char*l=0);

    protected:	// NEW (STR#1895)
        typedef struct
        {
            typedef struct { int x,y,w,h; } Fl_Region_XYWH;
            typedef struct { int l,r,t,b; } Fl_Region_LRTB;
            typedef struct { int x,y,w,h;
                             int pos;
                             int size;
                             int first;
                             int total;
            } Fl_Scrollbar_Data;

            int scrollsize;
            Fl_Region_XYWH innerbox;
            Fl_Region_XYWH innerchild;
            Fl_Region_LRTB child;
            int hneeded;
            int vneeded;
            Fl_Scrollbar_Data hscroll;
            Fl_Scrollbar_Data vscroll;
        } ScrollInfo;

    protected:
        void rearrangechildrens( int X, int Y, int W, int H );
        void recalc_scrollbars(ScrollInfo &si);
        void bbox(int&,int&,int&,int&);
        void draw();

    public:
        enum { // values for type()
            HORIZONTAL = 1,
            VERTICAL = 2,
            BOTH = 3,
            ALWAYS_ON = 4,
            HORIZONTAL_ALWAYS = 5,
            VERTICAL_ALWAYS = 6,
            BOTH_ALWAYS = 7
        };

    public:
        Fl_Scrollbar scrollbar;
        Fl_Scrollbar hscrollbar;

    public:
        void resize(int X, int Y, int W, int H);
        int  handle(int);
        void scroll_to(int, int);
        void clear();
        void separation(int W, int H);
        void autoarrange();

    public:
        int xposition()                     const { return xposition_;}
        int yposition()                     const { return yposition_;}
        int scrollbar_size()                const { return(scrollbar_size_);}
        void scrollbar_size(int newSize)    { if ( newSize != scrollbar_size_ ) redraw();
                                              scrollbar_size_ = newSize; }
        void bgimg( Fl_Image* i )           { imgbg = i; }

    private:
        int xposition_;
        int yposition_;
        int oldx, oldy;
        int scrollbar_size_;
        int xsep;
        int ysep;
        Fl_Image* imgbg;

    public:
        static void hscrollbar_cb(Fl_Widget*, void*);
        static void scrollbar_cb(Fl_Widget*, void*);
        void fix_scrollbar_order();
        static void draw_clip(void*,int,int,int,int);

};

#endif /// of __FL_NOBACKSCROLL_H__
