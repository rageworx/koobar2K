#ifndef __MPG123WRAP_H__
#define __MPG123WRAP_H__

#include "mpg123.h"

class MPG123Wrap
{
    public:
        MPG123Wrap();
        ~MPG123Wrap();

    public:
        bool Open( const char* path );
        void Close();

    public:
        unsigned DecodeFrame( unsigned char* &buffer, bool* nextavailed );
        void     SeekFrame( unsigned frmidx );
        void     DecodeFramePos( unsigned &curp, unsigned &maxp );

    public:
        bool GetTag( const char* scheme, unsigned char* &result, unsigned* tagsz );

    protected:
        const char* find_v2extras( const char* tname );
        bool find_v2imgage( char* &buffer, unsigned &buffersz );

    protected:
        mpg123_handle*  mh;
        unsigned        filesize;
        unsigned        filequeue;
        bool            opend;
        int             metainfo;
        off_t           framenum;
        off_t           frames_left;
        bool            fresh;
        bool            intflag;
        int             skip_tracks;

        long            new_header;
        size_t          minbytes;

        long            param_index_size;
        int             param_frame_number;
        int             param_frames_left;
        int             param_start_frame;
        int             param_checkrange;

        long            fmt_rate;
        int             fmt_bitrate;
        int             fmt_abr_rate;
        int             fmt_channels;
        int             fmt_encoding;
        int             fmt_vbr;

        mpg123_pars*    mp;
        struct\
        mpg123_frameinfo mp3info;
};

int  mpg123wrap_init();
void mpg123wrap_exit();

#endif // __MPG123WRAP_H__
