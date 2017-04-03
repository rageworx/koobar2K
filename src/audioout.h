#ifndef __AUDIOOUT_H__
#define __AUDIOOUT_H__

/***
** virtual class for audio out.
** -- prototype.
***/

class AudioOutEvent
{
    public:
        virtual void OnNewBuffer( unsigned position, unsigned nbsz, unsigned maxposition ) = 0;
        virtual void OnBufferEnd() = 0;
};

class AudioOut
{
    public:
        typedef enum
        {
            FAIL = -1,
            OK   = 0,
            ALEADYINITED,
            NEEDCONFIG,
        }AResult;

        typedef enum
        {
            NONE = -1,
            PLAY = 0,
            STOP,
        }ControlType;

    public:
        AudioOut(AudioOutEvent* e = 0) : _event( e ) {};
        ~AudioOut() {}

    public:
        virtual AResult InitAudio( unsigned chl, unsigned freq ) = 0;
        virtual AResult FinalAudio() = 0;
        virtual AResult ConfigAudio() = 0;

    public:
        virtual AResult WriteBuffer( const unsigned char* buffer, unsigned sz ) = 0;
        virtual unsigned BufferPosition() = 0;
        virtual AResult ClearBuffer() = 0;

    public:
        virtual AResult Control( ControlType ct, unsigned p1, unsigned p2 ) = 0;
        virtual ControlType ControlState() = 0;

    protected:
        AudioOutEvent*  _event;
};

#endif /// of __AUDIOOUT_H__
