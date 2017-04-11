#ifndef __AUDIODXSND_H__
#define __AUDIODXSND_H__

#include "audioout.h"
#include <windows.h>
#include <vector>

class AudioDXSound : public AudioOut
{
    public:
        AudioDXSound( HWND hParent, AudioOutEvent* e = NULL );
        ~AudioDXSound();

    public:
        AResult InitAudio( unsigned chl, unsigned freq );
        AResult FinalAudio();
        AResult ConfigAudio();

    public:
        AResult WriteBuffer( const unsigned char* buffer, unsigned sz );
        AResult FlushBuffer();
        AResult ClearBuffer();

    public:
        AResult Control( ControlType ct, unsigned p1, unsigned p2 );
        unsigned BufferPosition();
        ControlType ControlState() { return _ctrlstate; }

    public:
        DWORD ThreadCall();

    protected:
        bool createNextBuffer( bool flushleft = false );
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
        unsigned    _prevbufferque;
        void*       _dxbuff[2];
        int         _dxbidx_w;
        int         _dxbidx_r;
        HANDLE      _threadhandle;
        DWORD       _threadid;
        bool        _threadkeeplived;

    protected:
        std::vector< unsigned char > _buffer;
};

#endif // __AUDIODXSND_H__
