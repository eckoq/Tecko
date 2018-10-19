
#ifndef _TFC_BASE_SO_H_
#define _TFC_BASE_SO_H_

#include <dlfcn.h>
#include <string>

//////////////////////////////////////////////////////////////////////////

namespace tfc { namespace base {
	
	class CSOFile
	{
	public:
		CSOFile() : _handle(NULL){}
		~CSOFile()
		{
			if ( _handle ) {
				dlclose(_handle);
			}
			_handle = NULL;
		}
		
		int open(const std::string& so_file_path)
		{
			if (_handle)
			{
				dlclose(_handle);
				_handle = NULL;
			}
			_handle = dlopen (so_file_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
			if (_handle == NULL) {
				printf("dlopen: %s\n", dlerror());
				printf("./public/tfc/base:so_file_path.c_str(): %s\n", so_file_path.c_str());
			}
			assert(_handle);
			_file_path = so_file_path;
			
			return 0;
		}
		
		void* get_func(const std::string& func_name)
		{
			dlerror();    /* Clear any existing error */
			void* ret = dlsym(_handle, func_name.c_str());
			
			char *error;
			if ((error = dlerror()) != NULL)
				return NULL;
			else
				return ret;
		}
		
	private:
		std::string _file_path;
		void* _handle;
	};
	
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_BASE_SO_H_
///:~
