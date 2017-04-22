#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <dirent.h>

#include "dirsearch.h"

#include <locale>

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define DIR_SEP         '\\'
#else
#define DIR_SEP         '/'
#endif

////////////////////////////////////////////////////////////////////////////////

using namespace std;

////////////////////////////////////////////////////////////////////////////////

void struppers( string &src )
{
    locale loc;

    for ( string::size_type cnt=0; cnt<src.length(); cnt++)
        src[cnt] = toupper( src[cnt], loc );
}

////////////////////////////////////////////////////////////////////////////////

DirSearch::DirSearch( const char* rootdir, const char* endfix )
{
    _rootdir = rootdir;
    _endfix = endfix;

    dirsearch( rootdir, endfix );
}

DirSearch::~DirSearch()
{
    _filelist.clear();
}

void DirSearch::dirsearch( const char* rootdir, const char* endfix )
{
    struct dirent* pDE = NULL;

    DIR* pDIR = opendir( rootdir );
    if ( pDIR != NULL )
    {
        pDE = readdir( pDIR );

        while( pDE != NULL )
        {
            string fname = pDE->d_name;
            string fext;

            #ifdef DEBUG
            printf("%s\n", pDE->d_name);
            #endif // DEBUG

            struppers( fname );

            if ( endfix != NULL )
            {
                fext = endfix;
                struppers( fext );
            }

            if ( ( fname != "." ) && ( fname != ".." ) )
            {
                string cfname = rootdir;
                cfname += DIR_SEP;
                cfname += pDE->d_name;

                if ( DirTest( cfname.c_str() ) == true )
                {
                    dirsearch( cfname.c_str(), endfix );
                }

                struppers( fname );

                if ( fname.find( fext ) != string::npos )
                {
                    cfname = rootdir;
                    cfname += DIR_SEP;
                    cfname += pDE->d_name;

                    _filelist.push_back( cfname );
                }
            }
            pDE = readdir( pDIR );
        }

        closedir( pDIR );
    }
}

bool DirSearch::DirTest( const char* dir )
{
    DIR* pDIR = opendir( dir );
    if ( pDIR != NULL )
    {
        closedir( pDIR );

        return true;
    }

    return false;
}
