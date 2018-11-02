说明：
测试用echo_server，与conn_test配合使用。
echo_server监听配置端口，接受连接，并返回(echo)从连接上收到的数据。
简单演示CCD包完整性检查函数的使用。
(注意：需要使用dist.sh把框架打包后，才可以make)

使用方法：
1. 编译与安装
cd src
make
make install

2. 运行
可根据需要修改配置文件(如配置连接个数)
cd tools/op
./start.sh

3. 停止
cd tools/op
./stop.sh
