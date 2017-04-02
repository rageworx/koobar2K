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

MPG123Wrap::MPG123Wrap()
 : mh( NULL ),
   networked( false ),
   metainfo( 0 ),
   fresh( false ),
   intflag( false ),
   mp( NULL ),
   id3tv1( NULL ),
   id3tv2( NULL )
{
    result = mpg123_init();
    mp = mpg123_new_pars( &result );
    mh = mpg123_parnew( mp, "auto", &result );

    if ( mp != NULL )
    {
        mpg123_delete_pars(mp);
    }

    mpg123_param( mh, MPG123_ADD_FLAGS, MPG123_PICTURE, 0.);
}

MPG123Wrap::~MPG123Wrap()
{
    mpg123_exit();
}

bool MPG123Wrap::Open( const char* path )
{
    if( MPG123_OK != mpg123_param( mh, MPG123_ICY_INTERVAL, 0, 0) )
    {
        return false;
    }

    if ( strncmp( path, "http://", 7) == 0 ) /* http stream */
	{
        networked = true;
	}

    if ( mpg123_open( mh, path ) == MPG123_OK )
    {
        unsigned prevs = mpg123_tell( mh );

        mpg123_seek( mh, 0, SEEK_SET );
        metainfo = mpg123_meta_check(mh);

        if( ( metainfo & MPG123_ID3 ) != 0 )
        {
            int reti = mpg123_id3( mh, &id3tv1, &id3tv2 );

#ifdef DEBUG
            if ( reti == MPG123_OK )
            {
                print_v2( id3tv2 );
            }
            else
            {
                printf("Error: ID3Tag load failure!\n");
            }
#endif // DEBUG
        }

        mpg123_seek( mh, prevs, SEEK_SET );

        return true;
    }

    return false;
}

void MPG123Wrap::Close()
{
    if ( metainfo > 0 )
    {
        mpg123_meta_free(mh);
        metainfo = 0;
    }

    if ( mpg123_close( mh ) == MPG123_OK )
    {
        if ( networked == true )
        {
            networked = false;
        }
    }
}

unsigned MPG123Wrap::DecodeFrame( unsigned char* &buffer, bool* nextavailed )
{
    int             mc = -999;
    size_t          bytes = 0;

    while ( mc < 0 )
    {
        mc = mpg123_decode_frame(mh, &framenum, &buffer, &bytes);
        //mpg123_getstate( mh, MPG123_FRESH_DECODER, &new_header, NULL );

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
            case MPG123_DONE:
            case MPG123_ERR:
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

const char* MPG123Wrap::find_v2extras( const char* tname )
{
    if ( id3tv2 == NULL )
        return NULL;

    string tagname = tname;

	for( int cnt=0; cnt<id3tv2->texts; cnt++ )
	{
	    string id;
	    string lang;

        id = id3tv2->text[ cnt ].id;
        lang = id3tv2->text[ cnt].lang;

        if ( tagname == id )
        {
            return id3tv2->text[ cnt ].description.p;
        }
	}

	for( int cnt=0; cnt<id3tv2->extras; cnt++ )
	{
	    string id = id3tv2->extra[ cnt ].id;

	    if ( tagname == id )
        {
            if ( id3tv2->extra[ cnt ].description.fill > 0 )
            {
                return id3tv2->extra[ cnt ].description.p;
            }
        }
	}

	for( int cnt=0; cnt<id3tv2->comments; cnt++ )
	{
	    string id;
	    string lang;

        id = id3tv2->text[ cnt ].id;
        lang = id3tv2->text[ cnt].lang;

        if ( id == tagname )
        {
            if ( id3tv2->comment_list[ cnt ].description.fill > 0 )
                return id3tv2->comment_list[ cnt ].description.p;
        }
    }

	return NULL;
}

bool MPG123Wrap::find_v2imgage( char* &buffer, unsigned &buffersz )
{
    if ( id3tv2 == NULL )
        return NULL;

	for( int cnt=0; cnt<id3tv2->pictures; cnt++ )
	{
		mpg123_picture* pic = &id3tv2->picture[ cnt ];

		string imime = pic->mime_type.p;

		if ( imime == "image/jpeg" )
        {
            buffer = (char*)pic->data;
            buffersz = (unsigned)pic->size;

            return true;
        }
	}

	return false;
}

bool MPG123Wrap::GetTag( const char* scheme, unsigned char* &result, unsigned* tagsz )
{
    if ( mh != NULL )
    {
        if ( id3tv2 == NULL )
            return false;

        if ( scheme != NULL )
        {
            string scm = scheme;
            string retstr;

            if ( scm == "title" )
            {
                if ( id3tv2->title != NULL )
                {
                    if ( id3tv2->title->p != NULL )
                    {
                        retstr = id3tv2->title->p;
                    }
                }
            }
            else
            if ( scm == "artist" )
            {
                if ( id3tv2->artist != NULL )
                {
                    if ( id3tv2->artist->p != NULL )
                    {
                        retstr = id3tv2->artist->p;
                    }
                }
            }
            else
            if ( scm == "album" )
            {
                if ( id3tv2->album != NULL )
                {
                    if ( id3tv2->album->p != NULL )
                    {
                        retstr = id3tv2->album->p;
                    }
                }
            }
            else
            if ( scm == "year" )
            {
                if ( id3tv2->year != NULL )
                {
                    if ( id3tv2->year->p != NULL )
                    {
                        retstr = id3tv2->year->p;
                    }
                }
            }
            else
            if ( scm == "genre" )
            {
                if ( id3tv2->genre != NULL )
                {
                    if ( id3tv2->genre->p != NULL )
                    {
                        retstr = id3tv2->genre->p;
                    }
                }
            }
            else
            if ( scm == "track" )
            {
                const char* test = find_v2extras( "TRCK" );
                if ( test != NULL )
                {
                    retstr = test;
                }
            }
            else
            if ( scm == "art" )
            {
                char* imgbuff = NULL;
                unsigned imgsz = 0;

                if ( find_v2imgage( imgbuff, imgsz ) == true )
                {
                    unsigned char* imgdup = new unsigned char[imgsz];
                    if ( imgdup != NULL )
                    {
                        memcpy( imgdup, imgbuff, imgsz );
                        result = imgdup;
                        *tagsz = imgsz;

                        return true;
                    }
                }
            }

            if ( retstr.size() > 0 )
            {
                char* rets = strdup( retstr.data() );
                result = (unsigned char*)rets;
                *tagsz  = strlen( rets );

                return true;
            }
        }
    }
    return false;
}
