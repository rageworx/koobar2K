#include <FL/Fl.H>
#include <FL/Fl_Tiled_Image.H>
#include <FL/Fl_NobackScroll.H>
#include <FL/fl_draw.H>
#include "fl/Fl_NobackScroll.H"

Fl_NobackScroll::Fl_NobackScroll(int X,int Y,int W,int H,const char* L)
 : Fl_Group(X,Y,W,H,L),
   scrollbar(X+W-Fl::scrollbar_size(),Y, Fl::scrollbar_size(),H-Fl::scrollbar_size()),
   hscrollbar(X,Y+H-Fl::scrollbar_size(), W-Fl::scrollbar_size(),Fl::scrollbar_size()),
   xsep( 10 ),
   ysep( 10 )
{
    type( BOTH );

    xposition_ = oldx = 0;
    yposition_ = oldy = 0;
    scrollbar_size_ = 0;
    hscrollbar.type(FL_HORIZONTAL);

    hscrollbar.callback(hscrollbar_cb);
    scrollbar.callback(scrollbar_cb);
}

void Fl_NobackScroll::clear()
{
    remove(scrollbar);
    remove(hscrollbar);
    Fl_Group::clear();
    add(hscrollbar);
    add(scrollbar);
}

void Fl_NobackScroll::separation(int W, int H)
{
    xsep = W;
    ysep = H;

    autoarrange();
}

void Fl_NobackScroll::fix_scrollbar_order()
{
    Fl_Widget** a = (Fl_Widget**)array();

    if (a[children()-1] != &scrollbar)
    {
        int i,j;

        for (i = j = 0; j < children(); j++)
            if (a[j] != &hscrollbar && a[j] != &scrollbar) a[i++] = a[j];

        a[i++] = &hscrollbar;
        a[i++] = &scrollbar;
    }
}


void Fl_NobackScroll::draw_clip(void* v,int X, int Y, int W, int H)
{
    fl_push_clip(X,Y,W,H);
    Fl_NobackScroll* s = (Fl_NobackScroll*)v;

    Fl_Widget*const* a = s->array();
    for (int i=s->children()-2; i--;)
    {
        Fl_Widget& o = **a++;
        s->draw_child(o);
        s->draw_outside_label(o);
    }
    fl_pop_clip();
}

void Fl_NobackScroll::rearrangechildrens(int X, int Y, int W, int H)
{
    fix_scrollbar_order();

    int nchildren = children() - 2;

    if ( nchildren < 1 )
        return;

    int sw = ( scrollbar.visible() ) ? scrollbar.w() : 0;

    int cidx = nchildren - 1;

    if ( cidx < 0 )
        cidx = 0;

    int itm_w   = child( cidx )->w();
    int itm_h   = child( cidx )->h();

    int thumb_w = itm_w + xsep;
    int thumb_h = itm_h + ysep;
    int nw      = (W - sw - xsep) / thumb_w;
    int remain  = (W - sw - xsep) % thumb_w;

    if ( nw < 1 )
        nw = 1;

    //int left = X + ( remain / 2 );
    int left = X + xsep / nw;
    int top  = Y + ysep;

    int i_x  = left;
    int i_y  = top;

    for ( int i=0; i<nchildren; i++ )
    {
        Fl_Widget *w = child(i);

        w->position( i_x, i_y );

        if ( (i % nw) == (nw - 1) )
        {
            i_y += thumb_h;
            i_x = left;
        }
        else
        {
            i_x += thumb_w;
        }
    }
}

void Fl_NobackScroll::autoarrange()
{
    rearrangechildrens( x(), y(), w(), h() );
    redraw();
}

void Fl_NobackScroll::recalc_scrollbars(ScrollInfo &si)
{
    // inner box of widget (excluding scrollbars)
    si.innerbox.x = x()+Fl::box_dx(box());
    si.innerbox.y = y()+Fl::box_dy(box());
    si.innerbox.w = w()-Fl::box_dw(box());
    si.innerbox.h = h()-Fl::box_dh(box());

    // accumulate a bounding box for all the children
    si.child.l = si.innerbox.x;
    si.child.r = si.innerbox.x;
    si.child.b = si.innerbox.y;
    si.child.t = si.innerbox.y;

    int first = 1;

    Fl_Widget*const* a = array();

    for (int i=children(); i--;)
    {
        Fl_Widget* o = *a++;
        if ( o==&scrollbar || o==&hscrollbar ) continue;
        if ( first )
        {
            first = 0;
            si.child.l = o->x();
            si.child.r = o->x()+o->w();
            si.child.b = o->y()+o->h();
            si.child.t = o->y();
        } else {
            if (o->x() < si.child.l) si.child.l = o->x();
            if (o->y() < si.child.t) si.child.t = o->y();
            if (o->x()+o->w() > si.child.r) si.child.r = o->x()+o->w();
            if (o->y()+o->h() > si.child.b) si.child.b = o->y()+o->h();
        }
    }

    // Turn the scrollbars on and off as necessary.
    // See if children would fit if we had no scrollbars...
    {
        int X = si.innerbox.x;
        int Y = si.innerbox.y;
        int W = si.innerbox.w;
        int H = si.innerbox.h;

        si.scrollsize = scrollbar_size_ ? scrollbar_size_ : Fl::scrollbar_size();
        si.vneeded = 0;
        si.hneeded = 0;

        if (type() & VERTICAL)
        {
            if ((type() & ALWAYS_ON) || si.child.t < Y || si.child.b > Y+H)
            {
                si.vneeded = 1;
                W -= si.scrollsize;
                if (scrollbar.align() & FL_ALIGN_LEFT) X += si.scrollsize;
            }
        }

        if (type() & HORIZONTAL)
        {
            if ((type() & ALWAYS_ON) || si.child.l < X || si.child.r > X+W)
            {
                si.hneeded = 1;
                H -= si.scrollsize;
                if (scrollbar.align() & FL_ALIGN_TOP) Y += si.scrollsize;
                // recheck vertical since we added a horizontal scrollbar
                if (!si.vneeded && (type() & VERTICAL))
                {
                    if ((type() & ALWAYS_ON) || si.child.t < Y || si.child.b > Y+H)
                    {
                        si.vneeded = 1;
                        W -= si.scrollsize;
                        if (scrollbar.align() & FL_ALIGN_LEFT) X += si.scrollsize;
                    }
                }
            }
        }

        si.innerchild.x = X;
        si.innerchild.y = Y;
        si.innerchild.w = W;
        si.innerchild.h = H;
    }

    // calculate hor scrollbar position
    si.hscroll.x = si.innerchild.x;
    si.hscroll.y = (scrollbar.align() & FL_ALIGN_TOP) ? si.innerbox.y
                   : si.innerbox.y + si.innerbox.h - si.scrollsize;
    si.hscroll.w = si.innerchild.w;
    si.hscroll.h = si.scrollsize;

    // calculate ver scrollbar position
    si.vscroll.x = (scrollbar.align() & FL_ALIGN_LEFT) ? si.innerbox.x
                   : si.innerbox.x + si.innerbox.w - si.scrollsize;
    si.vscroll.y = si.innerchild.y;
    si.vscroll.w = si.scrollsize;
    si.vscroll.h = si.innerchild.h;

    // calculate h/v scrollbar values (pos/size/first/total)
    si.hscroll.pos = si.innerchild.x - si.child.l;
    si.hscroll.size = si.innerchild.w;
    si.hscroll.first = 0;
    si.hscroll.total = si.child.r - si.child.l;

    if ( si.hscroll.pos < 0 )
    {
        si.hscroll.total += (-si.hscroll.pos); si.hscroll.first = si.hscroll.pos;
    }

    si.vscroll.pos = si.innerchild.y - si.child.t;
    si.vscroll.size = si.innerchild.h;
    si.vscroll.first = 0;
    si.vscroll.total = si.child.b - si.child.t;

    if ( si.vscroll.pos < 0 )
    {
        si.vscroll.total += (-si.vscroll.pos); si.vscroll.first = si.vscroll.pos;
    }
}

void Fl_NobackScroll::bbox(int& X, int& Y, int& W, int& H)
{
    X = x()+Fl::box_dx(box());
    Y = y()+Fl::box_dy(box());
    W = w()-Fl::box_dw(box());
    H = h()-Fl::box_dh(box());

    if (scrollbar.visible())
    {
        W -= scrollbar.w();
        if (scrollbar.align() & FL_ALIGN_LEFT) X += scrollbar.w();
    }

    if (hscrollbar.visible())
    {
        H -= hscrollbar.h();
        if (scrollbar.align() & FL_ALIGN_TOP) Y += hscrollbar.h();
    }
}

void Fl_NobackScroll::draw()
{
    fix_scrollbar_order();

    int X,Y,W,H;
    bbox(X,Y,W,H);

    //fl_color( color() );
    //fl_rectf( X, Y, W, H );

    uchar d = FL_DAMAGE_ALL;

    draw_clip(this, X, Y, W, H);

    // Calculate where scrollbars should go, and draw them
    {
        ScrollInfo si;
        recalc_scrollbars(si);

        // Now that we know what's needed, make it so.
        if (si.vneeded && !scrollbar.visible())
        {
            scrollbar.set_visible();
            d = FL_DAMAGE_ALL;
        }
        else
        if (!si.vneeded && scrollbar.visible())
        {
            scrollbar.clear_visible();
            draw_clip(this, si.vscroll.x, si.vscroll.y, si.vscroll.w, si.vscroll.h);
            d = FL_DAMAGE_ALL;
        }

        if (si.hneeded && !hscrollbar.visible())
        {
            hscrollbar.set_visible();
            d = FL_DAMAGE_ALL;
        }
        else
        if (!si.hneeded && hscrollbar.visible())
        {
            hscrollbar.clear_visible();
            draw_clip(this, si.hscroll.x, si.hscroll.y, si.hscroll.w, si.hscroll.h);
            d = FL_DAMAGE_ALL;
        }
        else
        if ( hscrollbar.h() != si.scrollsize || scrollbar.w() != si.scrollsize )
        {
            // scrollsize changed
            d = FL_DAMAGE_ALL;
        }

        scrollbar.resize(si.vscroll.x, si.vscroll.y, si.vscroll.w, si.vscroll.h);
        oldy = yposition_ = si.vscroll.pos;	// si.innerchild.y - si.child.t;
        scrollbar.value(si.vscroll.pos, si.vscroll.size, si.vscroll.first, si.vscroll.total);

        hscrollbar.resize(si.hscroll.x, si.hscroll.y, si.hscroll.w, si.hscroll.h);
        oldx = xposition_ = si.hscroll.pos;	// si.innerchild.x - si.child.l;
        hscrollbar.value(si.hscroll.pos, si.hscroll.size, si.hscroll.first, si.hscroll.total);
    }

    draw_child(scrollbar);
    draw_child(hscrollbar);

    if (scrollbar.visible() && hscrollbar.visible())
    {
        // fill in the little box in the corner
        fl_color(color());
        fl_rectf(scrollbar.x(), hscrollbar.y(), scrollbar.w(), hscrollbar.h());
    }
}

void Fl_NobackScroll::resize(int X, int Y, int W, int H)
{
    rearrangechildrens( X, Y, W, H );

    int dx = X-x(), dy = Y-y();
    int dw = W-w(), dh = H-h();

    Fl_Widget::resize(X,Y,W,H);

    fix_scrollbar_order();
    // move all the children:
    Fl_Widget*const* a = array();
    for (int i=children()-2; i--;)
    {
        Fl_Widget* o = *a++;
        o->position(o->x()+dx, o->y()+dy);
    }

    if (dw==0 && dh==0)
    {
        char pad = ( scrollbar.visible() && hscrollbar.visible() );
        char al = ( (scrollbar.align() & FL_ALIGN_LEFT) != 0 );
        char at = ( (scrollbar.align() & FL_ALIGN_TOP)  !=0 );

        scrollbar.position(al?X:X+W-scrollbar.w(), (at&&pad)?Y+hscrollbar.h():Y);
        hscrollbar.position((al&&pad)?X+scrollbar.w():X, at?Y:Y+H-hscrollbar.h());
    }
    else
    {
        redraw();
    }
}

void Fl_NobackScroll::scroll_to(int X, int Y)
{
    int dx = xposition_-X;
    int dy = yposition_-Y;
    if (!dx && !dy) return;
    xposition_ = X;
    yposition_ = Y;
    Fl_Widget*const* a = array();
    for (int i=children(); i--;)
    {
        Fl_Widget* o = *a++;

        if (o == &hscrollbar || o == &scrollbar) continue;

        o->position(o->x()+dx, o->y()+dy);
    }

    if (parent() == (Fl_Group *)window() && Fl::scheme_bg_)
        damage(FL_DAMAGE_ALL);
    else
        damage(FL_DAMAGE_SCROLL);
}

void Fl_NobackScroll::hscrollbar_cb(Fl_Widget* o, void*)
{
    Fl_NobackScroll* s = (Fl_NobackScroll*)(o->parent());
    s->scroll_to(int(((Fl_Scrollbar*)o)->value()), s->yposition());
}

void Fl_NobackScroll::scrollbar_cb(Fl_Widget* o, void*)
{
    Fl_NobackScroll* s = (Fl_NobackScroll*)(o->parent());
    s->scroll_to(s->xposition(), int(((Fl_Scrollbar*)o)->value()));
}


int Fl_NobackScroll::handle(int event)
{
    fix_scrollbar_order();
    return Fl_Group::handle(event);
}
