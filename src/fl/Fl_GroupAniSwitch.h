#ifndef __FL_GROUPANISWITCH_H__
#define __FL_GROUPANISWITCH_H__

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>

/**
  The Fl_GroupAniSwitch class is the FLTK animation effector.
  (C)2017 Raphael Kim, Raph.K, (rageworx or rage.kim @gmail.com )

  ~ Disclaimer ~
  This class may have any bug.
  No warranty for using this class to your own project.
*/

class Fl_GroupAniSwitch
{
    public:
        typedef enum
        {
            ATYPE_JUSTHOW = 0,
            ATYPE_RIGHT2LEFT,
            ATYPE_LEFT2RIGHT,
            ATYPE_MAX
        }AnimationType;

    public:
        Fl_GroupAniSwitch( Fl_Group*, Fl_Group*,Fl_Group*,
                           AnimationType anitype = ATYPE_JUSTHOW,
                           bool autoHide = false,
                           unsigned ms = 1000 );
        ~Fl_GroupAniSwitch();

    public:
        void            WaitForFinish();
        void            NextStep(); /// may call by timer.
        void            Finish();
        Fl_RGB_Image*   BackgroundImage()   { return bg_blurred_img; }

    protected:
        int  calc_moves( int v1, int v2 );
        bool generate_img( Fl_Group* src, Fl_RGB_Image* &dst );
        bool generate_blurred_img( Fl_Group* src, Fl_RGB_Image* &dst );
        void discard_rgb_image( Fl_RGB_Image* &img );

    protected:
        typedef struct
        {   int x;
            int y;
        }GAC_POINT;

    protected:
        AnimationType   ani_type;
        Fl_Group*       grp_host;
        Fl_Group*       grp_src;
        Fl_Group*       grp_dst;
        Fl_RGB_Image*   bg_blurred_img;
        Fl_Group*       grp_tmp_bg;
        Fl_Box*         box_tmp_bg;
        float           draw_ms;
        GAC_POINT       src_org_pt;
        GAC_POINT       dst_org_pt;
        GAC_POINT       src_src_pt;
        GAC_POINT       src_dst_pt;
        GAC_POINT       dst_src_pt;
        GAC_POINT       dst_dst_pt;
        bool            draw_bg;
        bool            anim_null;
        bool            anim_finished;
        bool            auto_hide;
        unsigned        max_movel;
};

#endif /// of __FL_GROUPANISWITCH_H__
