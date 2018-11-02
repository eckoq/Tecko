#ifndef _STORAGE_BASE_CLOADGRID_H_
#define _STORAGE_BASE_CLOADGRID_H_

#include <string.h>
#include <sys/time.h>
#include <map>

namespace tfc{ namespace base {

	//////////////////////////////////////////////////////////////////////////
	class CLoadGrid
	{
		public:
			enum LoadRet
			{
				LR_NORMAL =0,
				LR_FULL = 1
			};

			CLoadGrid()
			{
				_grid_array = NULL;
			};

			CLoadGrid(unsigned grid_count
					, unsigned grid_distant
					, unsigned max_req_water_mark
					, timeval start_time)
			{
				_curr_grid = 0;
				_req_water_mark = 0;
				_start_time = start_time;
				_grid_count = grid_count;
				_grid_distant = grid_distant;
				_max_req_water_mark  = max_req_water_mark;
				_grid_array = new unsigned[_grid_count];
				memset(_grid_array, 0, _grid_count*sizeof(unsigned));

				_virtual_last_grid = 0;
			}
			void InitGrid(unsigned grid_count
					, unsigned grid_distant
					, unsigned max_req_water_mark
					, timeval start_time)
			{
				_curr_grid = 0;
				_req_water_mark = 0;
				_start_time = start_time;
				_grid_count = grid_count;
				_grid_distant = grid_distant;
				_max_req_water_mark  = max_req_water_mark;
				_grid_array = new unsigned[_grid_count];
				memset(_grid_array, 0, _grid_count*sizeof(unsigned));

				_virtual_last_grid = 0;
			}

			void Release()
			{
				if (_grid_array!=NULL) delete []_grid_array;
			}

			~CLoadGrid()
			{ 
				if (_grid_array!=NULL) delete []_grid_array;
			}

			void set_full(timeval cur_time)
			{
				_start_time = cur_time;
				_curr_grid = 0;
				memset(_grid_array, 0, _grid_count*sizeof(unsigned));
				_virtual_last_grid = 0;
				_grid_array[_grid_count - 1] = _max_req_water_mark;
				_req_water_mark = _max_req_water_mark;
			}

			int check_load(timeval cur_time)
			{
				double time_used = (cur_time.tv_sec -_start_time.tv_sec )*1000000.0
					+ cur_time.tv_usec -_start_time.tv_usec;

				if (time_used <0)
				{
					// something error, reset!
					_start_time = cur_time;
					_curr_grid = 0;
					_req_water_mark = 0;
					memset(_grid_array, 0, _grid_count*sizeof(unsigned));
					_virtual_last_grid = 0;
					return 0;
				}

				_virtual_curr_grid = (unsigned(time_used/1000.0))/_grid_distant;
				_curr_grid = _virtual_curr_grid % _grid_count;

				if(_virtual_curr_grid != _virtual_last_grid)
				{
					//两次检测之间跨越的格子数，之间的需要清零
					unsigned grid_spand = _virtual_curr_grid - _virtual_last_grid;
					_virtual_last_grid = _virtual_curr_grid;

					grid_spand = (grid_spand > _grid_count) ? _grid_count : grid_spand;
					for(unsigned i=0; i< grid_spand; i++)
					{
						_req_water_mark -= _grid_array[(_curr_grid - i + _grid_count)%_grid_count];
						_grid_array[(_curr_grid - i + _grid_count)%_grid_count] = 0;
					}


					if (_req_water_mark >= _max_req_water_mark)
					{
						return LR_FULL;
					}

				}

				_grid_array[_curr_grid] ++;
				_req_water_mark ++;
				if (_req_water_mark >= _max_req_water_mark)
				{
					_grid_array[_curr_grid] --;// delexxie 2009.02.18
					_req_water_mark --;//modify by delexxie 2009.02.18
					return LR_FULL;
				}

				return LR_NORMAL;
			}

			//modified by reganxie on 2007.1.5
			void fetch_load(unsigned& milliseconds, unsigned& req_cnt)
			{
				milliseconds = _grid_distant*_grid_count;
				req_cnt = _req_water_mark;
			}

            void fetch_new_load(timeval& cur_time, unsigned& milliseconds, unsigned& req_cnt)
            {
                double time_used = (cur_time.tv_sec -_start_time.tv_sec )*1000000.0
                    + cur_time.tv_usec -_start_time.tv_usec;

                if (time_used <0)
                {
                    // something error, reset!
                    _start_time = cur_time;
                    _curr_grid = 0;
                    _req_water_mark = 0;
                    memset(_grid_array, 0, _grid_count*sizeof(unsigned));
                    _virtual_last_grid = 0;
                }
                else
                {
                    _virtual_curr_grid = (unsigned(time_used/1000.0))/_grid_distant;
                    _curr_grid = _virtual_curr_grid % _grid_count;

                    if(_virtual_curr_grid != _virtual_last_grid)
                    {
                        //两次检测之间跨越的格子数，之间的需要清零
                        unsigned grid_spand = _virtual_curr_grid - _virtual_last_grid;
                        _virtual_last_grid = _virtual_curr_grid;

                        grid_spand = (grid_spand > _grid_count) ? _grid_count : grid_spand;
                        for(unsigned i=0; i< grid_spand; i++)
                        {
                            _req_water_mark -= _grid_array[(_curr_grid - i + _grid_count)%_grid_count];
                            _grid_array[(_curr_grid - i + _grid_count)%_grid_count] = 0;
                        }
                    }
                }
                
                milliseconds = _grid_distant*_grid_count;
                req_cnt = _req_water_mark;
            }

			unsigned _grid_count;
			unsigned _grid_distant;
			unsigned _max_req_water_mark;
			unsigned* _grid_array;

			unsigned _req_water_mark;
			unsigned _curr_grid;

			unsigned _virtual_last_grid;
			unsigned _virtual_curr_grid;
			timeval _start_time;
	};
}}

#endif
