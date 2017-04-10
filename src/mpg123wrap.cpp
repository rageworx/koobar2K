#include <windows.h>

#include "mpg123wrap.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

////////////////////////////////////////////////////////////////////////////////

using namespace std;

////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void print_lines(const char* prefix, mpg123_string *inlines)
{
	size_t i;
	int hadcr = 0, hadlf = 0;
	char *lines = NULL;
	char *line  = NULL;
	size_t len = 0;

	if(inlines != NULL && inlines->fill)
	{
		lines = inlines->p;
		len   = inlines->fill;
	}
	else return;

	line = lines;
	for(i=0; i<len; ++i)
	{
		if(lines[i] == '\n' || lines[i] == '\r' || lines[i] == 0)
		{
			char save = lines[i]; /* saving, changing, restoring a byte in the data */
			if(save == '\n') ++hadlf;
			if(save == '\r') ++hadcr;
			if((hadcr || hadlf) && hadlf % 2 == 0 && hadcr % 2 == 0) line = "";

			if(line)
			{
				lines[i] = 0;
				printf("%s%s\n", prefix, line);
				line = NULL;
				lines[i] = save;
			}
		}
		else
		{
			hadlf = hadcr = 0;
			if(line == NULL) line = lines+i;
		}
	}
}

/* Print out the named ID3v2  fields. */
void print_v2(mpg123_id3v2 *v2)
{
	print_lines("Title: ",   v2->title);
	print_lines("Artist: ",  v2->artist);
	print_lines("Album: ",   v2->album);
	print_lines("Year: ",    v2->year);
	print_lines("Comment: ", v2->comment);
	print_lines("Genre: ",   v2->genre);
}
#endif // DEBUG

////////////////////////////////////////////////////////////////////////////////

static bool mpg123wrap_inited = false;
static int mpg123wrap_result = 0;

int mpg123wrap_init()
{
    if ( mpg123wrap_inited == true )
        return mpg123wrap_result;

    mpg123wrap_result = mpg123_init();
    mpg123wrap_inited = true;
}

void mpg123wrap_exit()
{
    if ( mpg123wrap_inited == true )
    {
        mpg123_exit();

        mpg123wrap_inited = false;
    }
}

////////////////////////////////////////////////////////////////////////////////

MPG123Wrap::MPG123Wrap()
 : mh( NULL ),
   opend( false ),
   metainfo( 0 ),
   fresh( false ),
   intflag( false ),
   minbytes( 0 ),
   mp( NULL ),
   param_frame_number( -1 ),
   param_frames_left( 0 ),
   param_start_frame( 0 ),
   param_checkrange( 1 )
{
    int result = mpg123wrap_init();
    mp = mpg123_new_pars( &result );
    mh = mpg123_parnew( mp, "auto", &result );

    if ( mp != NULL )
    {
        mpg123_getpar(mp, MPG123_INDEX_SIZE, &param_index_size, NULL);
        mpg123_delete_pars(mp);
    }
}

MPG123Wrap::~MPG123Wrap()
{
    //mpg123_exit();
}

bool MPG123Wrap::Open( const char* path )
{
    if( MPG123_OK != mpg123_param( mh, MPG123_ICY_INTERVAL, 0, 0) )
    {
        return false;
    }

	filesize = 0;
	filequeue = 0;

    if ( mpg123_open( mh, path ) == MPG123_OK )
    {
        param_frame_number = -1;
        param_frames_left = 0;
        param_start_frame = 0;
        param_checkrange = 1;

        framenum = 0;

        mpg123_getformat( mh, &fmt_rate, &fmt_channels, &fmt_encoding );
        fmt_encoding = MPG123_SAMPLESIZE( fmt_encoding );

        unsigned prevs = mpg123_tell( mh );

        mpg123_seek( mh, 0, SEEK_END );
        filesize = mpg123_tell( mh );

        mpg123_seek( mh, 0, SEEK_SET );

        opend = true;

        return true;
    }

    MessageBox( 0, "MPG123Wrap::Open( const char* path ) failure", "DEBUG", MB_ICONINFORMATION );

    return false;
}

void MPG123Wrap::Close()
{
    if ( opend == false )
        return;

    if ( metainfo > 0 )
    {
        mpg123_meta_free(mh);
        metainfo = 0;
    }

    if ( mpg123_close( mh ) == MPG123_OK )
    {
        opend = false;
    }
}

unsigned MPG123Wrap::DecodeFrame( unsigned char* &buffer, bool* nextavailed )
{
    int             mc = -999;
    size_t          bytes = 0;

    while ( mc < 0 )
    {
        mc = mpg123_decode_frame( mh, &framenum, &buffer, &bytes );
        mpg123_getstate( mh, MPG123_FRESH_DECODER, &new_header, NULL );

        filequeue = mpg123_tell( mh );

        if( bytes > 0 )
        {
            if( param_frame_number > -1 )
            {
                --frames_left;
            }

            if( ( fresh == true ) && ( framenum >= param_start_frame) )
            {
                fresh = false;
            }

            return bytes;
        }

        switch( mc )
        {
            case MPG123_NEW_FORMAT:
                {
                    mpg123_getformat( mh, &fmt_rate, &fmt_channels, &fmt_encoding );
                }

            case MPG123_ERR:
#ifdef DEBUG
                {
                    printf(" MPG123 Error : %s\n", mpg123_strerror(mh) );
                }
#endif // DEBUG
            case MPG123_DONE:
                {
                    *nextavailed = false;
                    return 0;
                }
                break;
        }

    }

    return 0;
}

void MPG123Wrap::SeekFrame( unsigned frmidx )
{

}

void MPG123Wrap::DecodeFramePos( unsigned &curp, unsigned &maxp )
{
    if ( mh != NULL )
    {
        maxp = filesize;
        curp = filequeue;
    }
}
