#include <stdio.h>
#include <unistd.h>
#include <string>
#include <sys/stat.h>
#include <stddef.h>
#include <iostream>
#include <stdint.h>


int32_t mkdir_p(const std::string &filepath)
{
    std::string path;

    size_t pos = filepath.find_last_of("/");

    if (pos != (filepath.size() - 1))
        path = filepath + "/";
    else
        path = filepath;
    
    struct stat sb;
    if ( stat(path.c_str(), &sb) == 0 )
    {
        if ( S_ISDIR (sb.st_mode) ) 
            return 0;
    }
    
    std::string tmp;
    std::string::iterator it = path.begin();
    for ( ++it; it != path.end(); it++)
    {
        if ( *it == '/' )
        {
            tmp.assign(path.begin(), it);
            if ( stat(tmp.c_str(), &sb) != 0 )
            {
                if ( mkdir(tmp.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) < 0 )
                    return -1;
            }
            else if ( !S_ISDIR(sb.st_mode) ) {
                return -1;
            }
             
        }
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    std::string filepath = "/tmp/eckoqzhang/hello/world/aa.txt";
    
    size_t pos = filepath.find_last_of("/");
    std::string path = filepath.substr(0, pos);
    std::cout<<mkdir_p(path)<<std::endl;
    return 0;    
}
