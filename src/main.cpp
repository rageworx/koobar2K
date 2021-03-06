#include <cstdio>
#include <fcntl.h>

#include "wMain.h"

#include <FL/Fl.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_ask.H>

#include "mpg123wrap.h"

#if ( FL_ABI_VERSION < 10304 )
    #error "Error, FLTK ABI need 1.3.3 or above"
#endif

////////////////////////////////////////////////////////////////////////////////

#define DEF_APP_CLSNAME         "fm123gui"

////////////////////////////////////////////////////////////////////////////////

const char* reffntshape = NULL;

////////////////////////////////////////////////////////////////////////////////

void procAutoLocale()
{
    LANGID currentUIL = GetSystemDefaultLangID();

    const char* convLoc = NULL;

    switch( currentUIL & 0xFF )
    {
        case LANG_KOREAN:
            convLoc = "korean";
            reffntshape = "Malgun Gothic";
            break;

        case LANG_JAPANESE:
            convLoc = "japanese";
            reffntshape = "Meiryo";
            break;

        case LANG_CHINESE_SIMPLIFIED:
            convLoc = "chinese-simplified";
            reffntshape = "Microsoft YaHei";
            break;

        case LANG_CHINESE_TRADITIONAL:
            convLoc = "chinese-traditional";
            reffntshape = "Microsoft JhengHei";
            break;

        default:
            convLoc = "C";
            reffntshape = "Tahoma";
            break;
    }

#ifdef DEBUG
    printf( "current LANG ID = %08X ( %d ), locale = %s\n",
            currentUIL, currentUIL, convLoc );
#endif // DEBUG

    setlocale( LC_ALL, convLoc );
}

void presetFLTKenv()
{
    Fl::set_font( FL_FREE_FONT, reffntshape );
    Fl_Double_Window::default_xclass( DEF_APP_CLSNAME );

    fl_message_font_ = FL_FREE_FONT;
    fl_message_size_ = 11;

    Fl_Tooltip::color( fl_darker( FL_DARK3 ) );
    Fl_Tooltip::textcolor( FL_WHITE );
    Fl_Tooltip::size( 11 );
    Fl_Tooltip::font( FL_FREE_FONT );

    Fl::scheme( "flat" );
}

int main (int argc, char ** argv)
{
    int reti = 0;

    procAutoLocale();
    presetFLTKenv();

    mpg123wrap_init();

#ifdef R_DEBUG
    AllocConsole();

    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((long) handle_out, _O_TEXT);
    FILE* hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    *stdout = *hf_out;

    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((long) handle_in, _O_TEXT);
    FILE* hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 128);
    *stdin = *hf_in;
#endif // R_DEBUG

    wMain* pWMain = new wMain( argc, argv );

    if ( pWMain != NULL )
    {
        reti = pWMain->Run();
    }

    mpg123wrap_exit();

    return reti;
}
