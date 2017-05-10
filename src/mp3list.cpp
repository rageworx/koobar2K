#include "mp3list.h"
#include "dirsearch.h"

#include "FL/fl_ask.H"

#include <cstring>
#include <algorithm>

#include "mpg123wrap.h"

////////////////////////////////////////////////////////////////////////////////

using namespace std;

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
    #define DIR_SEP     '\\'
#else
    #define DIR_SEP     '/'
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Mp3List::Mp3List()
 : mpg123_result( -1 ),
   mhandle( NULL )
{
    mpars = mpg123_new_pars( &mpg123_result );
    mhandle = mpg123_parnew( mpars, "auto", &mpg123_result );

    if ( mpars != NULL )
    {
        long int pisz = 0;
        mpg123_getpar(mpars, MPG123_INDEX_SIZE, &pisz, NULL);
        mpg123_delete_pars(mpars);
    }

    mpg123_param( mhandle, MPG123_ADD_FLAGS, MPG123_PICTURE, 0.);
}

Mp3List::~Mp3List()
{
    Clear();
}

void Mp3List::Clear()
{
    unsigned rsz = _list.size();

    if ( rsz > 0 )
    {
        for ( list< Mp3Item* >::const_iterator it = _list.begin(), ep = _list.end();
              it != ep; it++ )
        {
            delete *it;
        }

        _list.clear();
    }

    _playindex.clear();

}

long Mp3List::AddListDir( const char* dir )
{
    long retl = 0;

    DirSearch* dsearch = new DirSearch( dir, ".mp3" );
    if ( dsearch != NULL )
    {
        vector< string >* plist = dsearch->data();
        if ( plist->size() > 0 )
        {
            for( unsigned cnt=0; cnt<plist->size(); cnt++ )
            {
                if( AddListFile( plist->at( cnt ).c_str() ) >= 0 )
                {
                    retl ++;
                }
            }
        }

        delete dsearch;

        return retl;
    }

    return -1;
}

long Mp3List::AddListFile( const char* fname )
{
    if ( fname != NULL )
    {
        string item = fname;

        if ( FindByFileName( fname ) == -1 )
        {
            Mp3Item* newitem = new Mp3Item;

            if ( newitem == NULL )
                return 0;

            //mpg123_param( mhandle, MPG123_ICY_INTERVAL, 0, 0);

            if ( mpg123_open( mhandle, fname ) == MPG123_OK )
            {
                newitem->filename( fname );

                struct mpg123_frameinfo mp3info;

                if ( mpg123_info( mhandle, &mp3info ) == MPG123_OK )
                {
                    switch( (int)mp3info.version )
                    {
                        case (int)MPG123_1_0:
                            newitem->version( 1.0f );
                            break;

                        case (int)MPG123_2_0:
                            newitem->version( 2.0f );
                            break;

                        case (int)MPG123_2_5:
                            newitem->version( 2.5f );
                            break;
                    }

                    newitem->layer( mp3info.layer );
                    newitem->mode( mp3info.mode );
                    newitem->bitratetype( mp3info.vbr );
                    newitem->bitrate( mp3info.bitrate );
                    newitem->abr( mp3info.abr_rate );
                }

                long int fmt_rate = 0;
                int fmt_channels = 0;
                int fmt_encoding = 0;

                mpg123_getformat( mhandle, &fmt_rate, &fmt_channels, &fmt_encoding );

                newitem->frequency( fmt_rate );
                newitem->channels( fmt_channels );
                newitem->encoding( MPG123_SAMPLESIZE( fmt_encoding ) );

                unsigned prevs = mpg123_tell( mhandle );

                mpg123_seek( mhandle, 0, SEEK_END );
                unsigned filesize = mpg123_tell( mhandle );
                newitem->filesize( filesize );

                mpg123_seek( mhandle, prevs, SEEK_SET );

                mpg123_id3v1* id3tv1 = NULL;
                mpg123_id3v2* id3tv2 = NULL;

                int metainfo = mpg123_meta_check( mhandle );

                if( ( metainfo & MPG123_ID3 ) != 0 )
                {
                    int reti = mpg123_id3( mhandle, &id3tv1, &id3tv2 );

                    if ( reti == MPG123_OK )
                    {
                        // Title.
                        if ( id3tv2->title != NULL )
                        {
                            if ( id3tv2->title->p != NULL )
                            {
                                newitem->title( id3tv2->title->p );
                            }
                        }
                        else
                        {
                            string tmpstr = fname;

                            string::size_type fpos = tmpstr.find_last_of( DIR_SEP );
                            if ( fpos != string::npos )
                            {
                                tmpstr = tmpstr.substr( fpos+1, tmpstr.size() - fpos );
                            }

                            fpos = tmpstr.find_last_of( '.' );
                            if ( fpos != string::npos )
                            {
                                tmpstr = tmpstr.substr( 0, fpos );
                            }

                            if ( tmpstr.size() > 0 )
                            {
                                newitem->title( tmpstr.c_str() );
                            }
                        }

                        // Artist
                        if ( id3tv2->artist != NULL )
                        {
                            if ( id3tv2->artist->p != NULL )
                            {
                                newitem->artist( id3tv2->artist->p );
                            }
                        }

                        // Album
                        if ( id3tv2->album != NULL )
                        {
                            if ( id3tv2->album->p != NULL )
                            {
                                newitem->album( id3tv2->album->p );
                            }
                        }

                        // Year
                        if ( id3tv2->year != NULL )
                        {
                            if ( id3tv2->year->p != NULL )
                            {
                                newitem->year( id3tv2->year->p );
                            }
                        }

                        // Genre
                        if ( id3tv2->genre != NULL )
                        {
                            if ( id3tv2->genre->p != NULL )
                            {
                                newitem->genre( id3tv2->genre->p );
                            }
                        }

                        // Track
                        const char* test = find_v2extras( id3tv2, "TRCK" );
                        if ( test != NULL )
                        {
                            newitem->track( test );
                        }

                        // Cover art.
                        for( int cnt=0; cnt<id3tv2->pictures; cnt++ )
                        {
                            mpg123_picture* pic = &id3tv2->picture[ cnt ];

                            string imime = pic->mime_type.p;

                            /* picture types ( ID3 v2.3.0 )
                            $00     Other
                            $01     32x32 pixels 'file icon' (PNG only)
                            $02     Other file icon
                            $03     Cover (front)
                            $04     Cover (back)
                            $05     Leaflet page
                            $06     Media (e.g. lable side of CD)
                            $07     Lead artist/lead performer/soloist
                            $08     Artist/performer
                            $09     Conductor
                            $0A     Band/Orchestra
                            $0B     Composer
                            $0C     Lyricist/text writer
                            $0D     Recording Location
                            $0E     During recording
                            $0F     During performance
                            $10     Movie/video screen capture
                            $11     A bright coloured fish
                            $12     Illustration
                            $13     Band/artist logotype
                            $14     Publisher/Studio logotype
                            */

                            // == cover art or other
                            if ( ( pic->type == 3 )  || ( pic->type == 0 ) )
                            if ( ( imime == "image/jpg" ) || ( imime == "image/jpeg" )
                                 || ( imime == "image/png" ) )
                            {
                                if ( newitem->coverart() == NULL )
                                {
                                    newitem->coverartmime( imime.c_str() );
                                    newitem->coverart( (char*)pic->data,
                                                       (unsigned)pic->size );
                                }
                            }
                        }
                    }
                }

                mpg123_close( mhandle );

                _list.push_back( newitem );
            }
            else
            {
                delete newitem;
            }
        }
    }
}

long Mp3List::FindByFileName( const char* fname )
{
    list< Mp3Item* >::const_iterator it = _list.begin();

    for( unsigned cnt=0; cnt<_list.size(); cnt++ )
    {
        Mp3Item* plist =  *it;
        if ( plist != NULL )
        {
            const char* diffname = plist->filename();
            if ( strcmp( diffname, fname ) == 0 )
            {
                return cnt;
            }
        }

        it++;
    }

    return -1;
}

long Mp3List::FindByTagTitle( const char* tagtitle )
{
    list< Mp3Item* >::const_iterator it = _list.begin();

    for( unsigned cnt=0; cnt<_list.size(); cnt++ )
    {
        Mp3Item* plist =  *it;
        if ( plist != NULL )
        {
            const char* diffname = plist->title();
            if ( strcmp( diffname, tagtitle ) == 0 )
            {
                return cnt;
            }
        }

        it++;
    }

    return -1;
}

const char* Mp3List::FileName( unsigned idx )
{
    if ( idx >= _list.size() )
        return NULL;

    list< Mp3Item* >::const_iterator it = _list.begin();

    advance( it, idx );

    Mp3Item* plist =  *it;

    return plist->filename();
}


Mp3Item* Mp3List::Get( unsigned idx )
{
    if ( idx >= _list.size() )
        return NULL;

    list< Mp3Item* >::const_iterator it = _list.begin();

    advance( it, idx );

    Mp3Item* plist =  *it;

    return plist;
}


void Mp3List::ShufflePlayIndex()
{
    // Let make shuttle LUT !
    if ( _list.size() > 0 )
    {
        _playindex.clear();
        _playindex.resize( _list.size() );

        for( unsigned cnt=0; cnt<_list.size(); cnt++ )
        {
            _playindex[ cnt ] = cnt;
        }

        std::random_shuffle( _playindex.begin(), _playindex.end() );

#ifdef DEBUG
        for( unsigned cnt=0; cnt<_list.size(); cnt++ )
        {
            printf("[%d] %d   ", cnt, _playindex[ cnt ] );
        }
        printf("\n");

#endif // DEBUG
    }
}

long Mp3List::FindPlayIndexToListIndex( unsigned idx )
{
    for ( unsigned cnt=0; cnt<_playindex.size(); cnt++ )
    {
        if ( _playindex[ cnt ] == idx )
            return cnt;
    }

    return -1;
}

const char* Mp3List::find_v2extras( void* p, const char* tname )
{
    if ( p == NULL )
        return NULL;

    mpg123_id3v2* id3tv2 = (mpg123_id3v2*)p;

    string tagname = tname;

	for( int cnt=0; cnt<id3tv2->texts; cnt++ )
	{
	    string id;
	    string lang;

        id = id3tv2->text[ cnt ].id;
        lang = id3tv2->text[ cnt].lang;

        if ( tagname == id )
        {
            return id3tv2->text[ cnt ].text.p;
        }
	}

	for( int cnt=0; cnt<id3tv2->extras; cnt++ )
	{
	    string id = id3tv2->extra[ cnt ].id;

	    if ( tagname == id )
        {
            return id3tv2->extra[ cnt ].text.p;
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
            return id3tv2->comment_list[ cnt ].text.p;
        }
    }

	return NULL;
}

