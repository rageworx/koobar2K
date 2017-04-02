#ifdef _WIN32
    #include <windows.h>
#endif /// of _WIN32

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "stdunicode.h"

#ifdef _WIN32

char *ConvertFromUnicode( const wchar_t *src)
{
    static char* dest = NULL;

    if ( ( dest != NULL ) || ( src == NULL ) )
    {
        delete[] dest;
        dest = NULL;
    }

    if ( src == NULL )
    {
        return NULL;
    }

    int len = WideCharToMultiByte( CP_ACP, 0, src, -1, NULL, 0, NULL, NULL );
    dest = (char*)calloc(len+1,1);

    WideCharToMultiByte(CP_ACP,0,src,-1,dest,len,NULL,NULL);

    return dest;
}

wchar_t *ConvertFromMBCS( const char *src)
{
    static wchar_t* dest = NULL;

    if ( ( dest != NULL ) || ( src == NULL ) )
    {
        delete[] dest;
        dest = NULL;
    }

    if ( src == NULL )
    {
        return NULL;
    }

    int len = strlen(src) + 1;
    dest = (wchar_t*)calloc(len * 2,1);
    MultiByteToWideChar(CP_ACP,0,src,-1,dest,len);

    return dest;
}

char* convertW2M(const wchar_t* src)
{
    return ConvertFromUnicode( src );
}

wchar_t* convertM2W(const char* src)
{
    return ConvertFromMBCS( src );
}

#else /// _WIN32

char* convertW2M(const wchar_t* src)
{
    static char* convM = NULL;

    if ( src == NULL )
    {
        if ( convM != NULL )
        {
            delete[] convM;
            convM = NULL;
        }
    }

    int sz  = wcslen( src );

    if ( convM != NULL )
    {
        delete[] convM;
        convM = NULL;
    }

    convM = new char[ sz + 1 ];
    if ( convM != NULL )
    {
        memset( convM, 0, sz + 1 );
        wcstombs( convM, src, sz );
    }

    return convM;
}

wchar_t* convertM2W(const char* src)
{
    static wchar_t* convW = NULL;

    if ( src == NULL )
    {
        if ( convW != NULL )
        {
            delete[] convW;
            convW = NULL;
        }
    }

    int sz  = strlen( src );

    if ( convW != NULL )
    {
        delete[] convW;
        convW = NULL;
    }

    convW = new wchar_t[ sz + 1 ];

    if ( convW != NULL )
    {
        memset( convW, 0, ( sz + 1 ) * sizeof(wchar_t) );
        mbstowcs( convW, src, sz * sizeof(wchar_t) );
    }

    return convW;
}
#endif /// _WIN32
