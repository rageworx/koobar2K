#ifndef __DIRSEARCH_H__
#define __DIRSEARCH_H__

#include <string>
#include <vector>

class DirSearch
{
    public:
        DirSearch( const char* rootdir, const char* endfix = NULL );
        ~DirSearch();

    public:
        std::vector< std::string >* data() { return &_filelist; }
        unsigned                    size() { return _filelist.size(); }

    protected:
        std::string                 _rootdir;
        std::string                 _endfix;
        std::vector< std::string >  _filelist;
};

#endif __DIRSEARCH_H__
