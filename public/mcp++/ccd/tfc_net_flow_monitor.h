/******************************************************************************
 * tfc_net_flow_monitor.h
 * Copyright 2012, Tencent Inc.
 * Author: Liu Shiyi <saintvliu@tencent.com>
 *
 * Description  : A simple data rate monitor for ConnCache
 * File created : 2012-12-05
 * 
 ******************************************************************************/
#ifndef _TFC_NET_FLOW_MONITOR_H_
#define _TFC_NET_FLOW_MONITOR_H_

#include <sys/time.h>

namespace tfc{ namespace net {

    //////////////////////////////////////////////////////////////////////////
    struct CFlowMonitor
    {
        void init(unsigned sample_count, unsigned interval)
        {
            _grid_count = sample_count;
            _grid_interval = interval;
            _grid_array = new unsigned[_grid_count];
            reset_stat();
        }

        void fini() { if(NULL != _grid_array) delete []_grid_array; }

        // reset before used
        inline void reset_stat()
        {
            _start_time.tv_sec = 0;
            _start_time.tv_usec = 0;
            _data_size = 0;
            memset(_grid_array, 0, _grid_count*sizeof(unsigned));
            _last_grid = 0;
        }

        inline unsigned touch(struct timeval* cur_time, unsigned data_len=0)
        {
            double time_used = ((cur_time->tv_sec - _start_time.tv_sec )*1000.0
                + (cur_time->tv_usec -_start_time.tv_usec)/1000.0);

            if (((0 == _start_time.tv_sec) && (0 == _start_time.tv_usec)) || (0 > time_used))
            {
                // 首次统计，先清零
                _start_time = *cur_time;
                memset(_grid_array, 0, _grid_count * sizeof(unsigned));
                _last_grid = 0;
                _grid_array[0] = data_len;
                _data_size = data_len;
                return (_data_size * 50000);
            }

            unsigned virtual_curr_grid = (unsigned)time_used/_grid_interval;
            unsigned curr_grid = virtual_curr_grid % _grid_count;

            time_used = time_used < (_grid_interval * _grid_count) ? time_used : (_grid_interval * _grid_count - _grid_interval + ((unsigned)time_used % _grid_interval));
            if (virtual_curr_grid != _last_grid)
            {
                // 可能在循环队列里面发生翻转，需要清除以前记录的格子
                unsigned grid_spand = virtual_curr_grid - _last_grid;
                _last_grid = virtual_curr_grid;

                grid_spand = (grid_spand > _grid_count) ? _grid_count : grid_spand;
                unsigned i, j;
                for(i=0; i< grid_spand; i++)
                {
                    j = (curr_grid - i + _grid_count) % _grid_count;
                    _data_size -= _grid_array[j];
                    _grid_array[j] = 0;
                }
            }

            if (data_len)
            {
                _grid_array[curr_grid] += data_len;
                _data_size += data_len;
            }

            return (unsigned)((double)_data_size * 1000 / time_used);
        }

        unsigned _grid_count;
        unsigned _grid_interval;
        unsigned* _grid_array;

        unsigned _data_size;

        unsigned _last_grid;
        struct timeval _start_time;
    };
}}


//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_FLOW_MONITOR_H_
///:~
