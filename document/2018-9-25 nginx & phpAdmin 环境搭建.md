---
title: 2018-9-25 nginx & phpAdmin 环境搭建
tags: nginx,phpAdmin
grammar_cjkRuby: true
---

## php安装流程
-----
1. phpMyAdmin源码下载：
	https://www.phpmyadmin.net/
	![enter description here](./images/1537872890198.png)
	
2. php下载
	http://php.net/get/php-5.6.38.tar.gz/from/a/mirror
	2.1 安装libxml2
	yum install libxml2
	yum install libxml2-devel -y
	
	2.2 编译php源码
	./configure --prefix=/usr/local/php  --enable-fpm

3. phpMyadmin
	3.1 phpMyAdmin-4.8.3-all-languages.zip 解压到nginx目录/usr/local/nginx/html
	3.2 修改libraries/config.default.php的host
	![enter description here](./images/1537929496219.png)
	3.3 配置nginx.conf文件
	![enter description here](./images/1537929596872.png)
	
## 异常
----
一、 缺少 mysqli 扩展，请检查 PHP 配置。
解决方案：
1. 进入/\*/php-5.6.38/ext/mysqli目录
2. 执行/usr/local/php/bin/phpize
3. ./configure --with-php-config=/usr/local/php/bin/php-config --with-mysqli=/usr/local/mysql/bin/mysql_config
4. make && make install
*异常问题：mysql_float_to_double.h No such file，修复如下*
![enter description here](./images/1537929055707.png)
5. 编辑/etc/php.ini，添加extension=mysqli.so
![enter description here](./images/1537929201318.png)
6. 重启php-fpm， kill掉进程后执行/usr/local/php/sbin/php-fpm