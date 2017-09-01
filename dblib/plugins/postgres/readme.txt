1.
如何编译libpq：
需要整个pg源码编译生成libpq.a

2.
FATAL:  no pg_hba.conf entry for host "192.168.63.40", user "postgres", database "postgres", SSL off

修改服务端配置文件：pg_hba.conf(无需重启即生效)
添加：
host    all         all         192.168.63.40/32      md5
