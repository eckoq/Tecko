/*
 * ***************************************   sample   *************************************
 * 1. 包含ccd文件夹下的over_ctrl.h头文件                                                  *
 * 2. 初始化overctrl模块，并传入相关初始化参数                                            *
 * 3. 判断请求在ccd停留时间，停留时间过长判断为过载，停留时间根据自身业务特性进行调整     *
 *   （非常重要！！，业务雪崩都是由于ccd管道满，处理大量无效请求导致 ）                   *
 * 4. 在ccd到mcd管道中dequeue请求数据时，使用overctrl模块进行过载判断                     *
 * 5. 使用mcp++的请求过载保护进行过载判断(该功能可以控制当前模块到后端模块的请求),可适当  *
 *    调整值保持较大，平时不启用                                                          *
 *                                                                                        *
 * 注：overctrl过载判断原理为：读取机器的总CPU负载与该进程当前的CPU负载，超过80%为过载    *
 *     读取每块网卡流量，千兆卡流量超过网卡流量瓶颈80%为过载，万兆卡超过3.5Gb/s为过载     *
 *     对每个请求判断为过载后，则应给请求方返回过载响应                                   *
 *     过载模块内部有响应的平滑过载处理机制，不会引起毛刺问题                             *
 *     通用过载 务必（！！！）与请求在管道的停留时间结合使用，确保模块不会产生雪崩        * 
 *                                                                                        *
 ****************************************************************************************** 
 */


/* 包含通用过载控制模块头文件    */
#include "ccd/over_ctrl.h"

int main(int argc, char **argv)
{
	/* 传入当前进程号，用于捕获当前进程cpu使用率 
	 * 该函数包含一个线程数，如果是单进程单线程，
	 * 按默认值设置，如果是多线程，务必保证所有线
	 * 程的计算量是一致的，否则cpu判断不准确
	 * 如果只有一个线程参与主循环或者计算，其他
	 * 线程负载很低，则按默认值设置即可。	 
	 * */
	RESSTAT->init(getpid());

	/* 设置CPU过载值     */
	OVER_CTRL->set_cpu_overload_rate(cpu_overload_rate);
	/* 设置网卡过载值    */
	OVER_CTRL->set_network_over_flow_rate(net_work_overflow_rate);

	/* 启动系统信息收集线程 */
	OVER_CTRL->start_thread();

	/* 判断请求在管道中停留的时间,过长直接返回过载，非常重要！！！ */
	TCCDHeader* head = (TCCDHeader*)buf;
	unsigned time = head->_timestamp_msec + head->_timestamp*1000;
	if(time>MQ_KEEP_TIME) {
		return OVER_LOAD;
	}

	m_mqCcd2Mcd->try_dequeue(buf,buf_len,data_len,flow);

	/* 判断是否过载 , refuse_rate 为拒绝百分比，暂时无用 */
    int refuse_rate = 0;
	bool is_over_load = OVER_CTRL->is_over_load(refuse_rate);
	
	if(is_over_load) {
		return OVER_LOAD;
	}
	return 0;
}
