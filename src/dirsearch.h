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
        static bool DirTest( const char* dir );

    public:
        std::vector< std::string >* data() { return &_filelist; }
        unsigned                    size() { return _filelist.size(); }

    protected:
        void dirsearch( const char* rootdir, const char* endfix );

    protected:
        std::string                 _rootdir;
        std::string                 _endfix;
        std::vector< std::string >  _filelist;
};

#endif /// of __DIRSEARCH_H__
