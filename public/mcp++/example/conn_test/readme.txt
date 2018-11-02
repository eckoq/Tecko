说明：
测试用client，与echo_server配合使用。
conn_test可创建多个连接向echo_server发送数据，接收echo_server返回的数据并统计包量。
简单演示DCC包完整性检查函数的使用。
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

3. 打开测试开关
cd tools/op
./change_state.sh

4. 停止
cd tools/op
./stop.sh

5. 查看统计信息
简单的统计信息可以查看
log/conn_test_mcd.log
