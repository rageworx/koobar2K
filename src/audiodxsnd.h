#ifndef __AUDIODXSND_H__
#define __AUDIODXSND_H__

#include "audioout.h"
#include <windows.h>
#include <pthread.h>
#include <vector>

class AudioDXSound : public AudioOut
{
    public:
        AudioDXSound( HWND hParent );
        ~AudioDXSound();

    public:
        AResult InitAudio( unsigned chl, unsigned freq );
        AResult FinalAudio();
        AResult ConfigAudio();

    public:
        AResult WriteBuffer( const unsigned char* buffer, unsigned sz );
        AResult ClearBuffer();

    public:
        AResult Control( ControlType ct, unsigned p1, unsigned p2 );
        ControlType ControlState() { return _ctrlstate; }

    public:
        void* ThreadCall();

    protected:
        bool createNextBuffer();
        bool procPlay();
        void procStop();

    protected:
        HWND        _hParent;
        bool        _inited;
        bool        _played;
        unsigned    _channels;
        unsigned    _frequency;
        ControlType _newctrlstate;
        ControlType _ctrlstate;
        unsigned    _currentbufferque;
        void*       _dxbuff[2];
        int         _dxbidx_w;
        int         _dxbidx_r;
        pthread_t   _ptt;
        bool        _threadkeeplived;

    protected:
        std::vector< unsigned char > _buffer;
};

#endif // __AUDIODXSND_H__
