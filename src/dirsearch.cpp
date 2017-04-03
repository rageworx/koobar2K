#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <dirent.h>

#include "dirsearch.h"

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define DIR_SEP         '\\'
#else
#define DIR_SEP         '/'
#endif

////////////////////////////////////////////////////////////////////////////////

using namespace std;

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

            if ( ( fname != "." ) && ( fname != ".." ) )
            {
                string cfname = rootdir;
                cfname += DIR_SEP;
                cfname += pDE->d_name;

                if ( DirTest( cfname.c_str() ) == true )
                {
                    string cfname = rootdir;
                    cfname += DIR_SEP;
                    cfname += pDE->d_name;

                    dirsearch( cfname.c_str(), endfix );
                }

                if ( fname.find( endfix ) != string::npos )
                {
                    cfname = rootdir;
                    cfname += DIR_SEP;
                    cfname += pDE->d_name;

#ifdef DEBUG
                    printf( "%s\n", cfname.c_str() );
#endif // DEBUG
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
