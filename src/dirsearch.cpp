#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <dirent.h>

#include "dirsearch.h"

////////////////////////////////////////////////////////////////////////////////

#define DIR_SEP         '/'

////////////////////////////////////////////////////////////////////////////////

using namespace std;

////////////////////////////////////////////////////////////////////////////////

DirSearch::DirSearch( const char* rootdir, const char* endfix )
{
    _rootdir = rootdir;
    _endfix = endfix;

    struct dirent* pDE = NULL;

    DIR* pDIR = opendir( rootdir );
    if ( pDIR != NULL )
    {
        pDE = readdir( pDIR );

        while( pDE != NULL )
        {
            string fname = pDE->d_name;

            if ( fname.find( endfix ) != string::npos )
            {
                string cfname = rootdir;
                cfname += DIR_SEP;
                cfname += pDE->d_name;

                printf( "%s\n", cfname.c_str() );

                _filelist.push_back( cfname );
            }

            pDE = readdir( pDIR );
        }

        closedir( pDIR );
    }

}

DirSearch::~DirSearch()
{
    _filelist.clear();
}
