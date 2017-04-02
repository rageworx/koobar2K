#ifndef __SAFERELEASE_H__
#define __SAFERELEASE_H__

/*
#ifndef SafeRelease
    #define SafeRelease( _X_ ) { if ( _X_ != NULL ) { delete _X_; _X_ = NULL; } }
#endif /// SaveRelease
*/

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

#endif /// of __SAFERELEASE_H__
