概述:
MCP是高性能大容量的网络服务器开发框架.主要有CCD, MCD, DCC, Watchdog 四个进程模块.
Watchdog负责其他三类进程的启动,停止,死锁检查等.Watchdog是其他进程的父进程,通过共享内存维持心跳.
CCD进程分别负责网络前端收发.
MCD进程通过加载业务共享SO负责业务逻辑处理.
DCC进程负责网络后端收发(模块主动发起的请求通过DCC发送).
CCD, MCD, DCC各个进程模块之间通过MQ连接.

特点:
高性能，大容量
网络处理和业务处理分离
业务处理异步化
开发简单(入门门槛较高)
通用性高
配置丰富

目录结构说明:
base      一些基础类库
old       一些基础类库
ccd       CCD源代码
dcc       DCC源代码
mcd       MCD源代码
libhttp   http处理库源代码
watchdog  watchdog源代码
example   MCD动态库编程例子，简单的Echo server
makefile  构建所有模块的makefile，模块有一定的顺序依赖，建议使用这个makefile构建所有模块
dist.sh   一键生成SDK

开始使用:
1. 一键生成SDK(编译框架代码并打包)
./dist.sh
2. 编译示例代码
cd mcp++_xxx/example/echo_server/src
make
make install
cd mcp++_xxx/example/conn_test/src
make
make install
3. 运行
cd mcp++_xxx/example/echo_server/tools/op
./start.sh
cd mcp++_xxx/example/conn_test/tools/op
./start.sh
./change_state.sh
tail log/conn_test_mcd.log 查看统计

开发和使用指南：
http://mcp.oa.com
http://mcp.oa.com/mcppp_manual/mcp++-manual.htm

bug反馈：
http://tapd.oa.com/mcp/bugtrace/bugreports/my_view