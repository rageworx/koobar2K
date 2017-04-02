#ifndef __AUDIOOUT_H__
#define __AUDIOOUT_H__

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
            PAUSE,
            RESUME,
            STOP,
            SEEK,
        }ControlType;

    public:
        AudioOut() {}
        ~AudioOut() {}

    public:
        virtual AResult InitAudio( unsigned chl, unsigned freq ) = 0;
        virtual AResult FinalAudio() = 0;
        virtual AResult ConfigAudio() = 0;

    public:
        virtual AResult WriteBuffer( const unsigned char* buffer, unsigned sz ) = 0;
        virtual AResult ClearBuffer() = 0;

    public:
        virtual AResult Control( ControlType ct, unsigned p1, unsigned p2 ) = 0;
        virtual ControlType ControlState() = 0;
};

#endif /// of __AUDIOOUT_H__
