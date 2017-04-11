#ifndef __MP3LIST_H__
#define __MP3LIST_H__

#include "mpg123.h"
#include <vector>
#include <list>
#include <string>

class Mp3Item
{
    public:
        Mp3Item() : _coverart(NULL), _vbr(0), _bitrate(1), _abr(0), _layer(0), \
                    _encoding(0), _frequency(44100), _channels(2), _filesize(0),\
                    _version(0)\
                                            { \
                                                _filename.clear();  \
                                                _track.clear();     \
                                                _title.clear();     \
                                                _artist.clear();    \
                                                _album.clear();     \
                                                _genre.clear();     \
                                                _year.clear();      \
                                                _mode = "NONE";      \
                                                _brtype.clear();    \
                                                _coverartmime = "image/none";   \
                                            }
        ~Mp3Item()                          { if ( _coverart != NULL ) \
                                                delete[] _coverart; }

    public:
        void filename( const char* s )      { _filename = s; }
        void track( const char* s )         { _track = s; }
        void title( const char* s )         { _title = s; }
        void album( const char* s )         { _album = s; }
        void artist( const char* s )        { _artist = s; }
        void genre( const char* s )         { _genre = s; }
        void year( const char* s )          { _year = s; }
        void mode( const unsigned n )       { if ( n < 4 ) _mode = strstro[ n ]; _mode = "NONE"; }
        void bitratetype( const unsigned n ){ if ( n < 3 ){ _brtype = strbrs[ n ]; _vbr=n; } }
        void bitrate( const unsigned n )    { _bitrate = n; }
        void abr( const unsigned n )        { _abr = n; }
        void layer( const unsigned n )      { _layer = n; }
        void encoding( const unsigned n )   { _encoding = n; }
        void frequency( const unsigned n )  { _frequency = n; }
        void channels( const unsigned n )   { _channels = n; }
        void filesize( const unsigned n )   { _filesize = n; }
        void coverart ( const char* i, unsigned sz ) \
                                            { if ( ( i == NULL ) || ( sz < 1 ) ) return; \
                                              if ( _coverart != NULL ) delete[] _coverart; \
                                              _coverart = new char[ sz ]; \
                                              if ( _coverart != NULL ) \
                                              { memcpy( _coverart, i, sz ); _coverartsize = sz; } }
        void coverartmime( const char* s )  { _coverartmime = s; }
        void version( const float f )       { _version = f; }

    public:
        const char* filename()              { return _filename.c_str(); }
        const char* track()                 { return _track.c_str(); }
        const char* title()                 { return _title.c_str(); }
        const char* artist()                { return _artist.c_str(); }
        const char* album()                 { return _album.c_str(); }
        const char* genre()                 { return _genre.c_str(); }
        const char* year()                  { return _year.c_str(); }
        const char* mode()                  { return _mode.c_str(); }
        const char* bitratetype()           { return _brtype.c_str(); }
        const unsigned vbr()                { return _vbr; }
        const unsigned bitrate()            { switch( _vbr ){ case MPG123_CBR:\
                                                              case MPG123_VBR:\
                                                                  return _bitrate;break; \
                                                              case MPG123_ABR:\
                                                                  return _abr; break;} }
        const unsigned layer()              { return _layer; }
        const unsigned encoding()           { return _encoding; }
        const unsigned frequency()          { return _frequency; }
        const unsigned channels()           { return _channels; }
        const unsigned filesize()           { return _filesize; }
        const char* coverart()              { return _coverart; }
        const unsigned coverartsize()       { return _coverartsize; }
        const char* coverartmime()          { return _coverartmime.c_str(); }
        const float version()               { return _version;}

    protected:
        const char* strstro[4] = { "stereo", "joint-stereo", "dual", "mono" };
        const char* strbrs[3]  = { "CBR", "VBR", "ABR" };

    protected:
        std::string _filename;
        std::string _track;
        std::string _title;
        std::string _artist;
        std::string _album;
        std::string _genre;
        std::string _year;
        std::string _mode;
        std::string _brtype;
        unsigned    _vbr;
        unsigned    _bitrate;
        unsigned    _abr;
        unsigned    _layer;
        unsigned    _encoding;
        unsigned    _frequency;
        unsigned    _channels;
        unsigned    _filesize;
        float       _version;
        char*       _coverart;
        std::string _coverartmime;
        unsigned    _coverartsize;
};

class Mp3List
{
    public:
        Mp3List();
        ~Mp3List();

    public:
        long     AddListDir( const char* dir = NULL );
        long     AddListFile( const char* fname = NULL );
        long     FindByFileName( const char* fname = NULL );
        long     FindByTagTitle( const char* tagtitle = NULL );
        const char* FileName( unsigned idx );
        Mp3Item*    Get( unsigned idx );

    public:
        void ShufflePlayIndex();
        long FindPlayIndexToListIndex( unsigned idx );

    public:
        unsigned Size()                         { return _list.size(); }
        unsigned PlayQueue( unsigned idx )      { if ( idx < _playindex.size() )\
                                                    return _playindex[ idx ]; }
    protected:
        const char* find_v2extras( void* p, const char* tname );

    protected:
        std::list< Mp3Item* >   _list;
        std::vector< unsigned > _playindex;

    protected:
        int             mpg123_result;
        mpg123_handle*  mhandle;
        mpg123_pars*    mpars;
};

#endif /// of __MP3LIST_H__
