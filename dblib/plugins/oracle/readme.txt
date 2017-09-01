Oracle是个人免费，企业收费的高性能数据库

0.
Oracle下载地址：
http://www.oracle.com/technetwork/database/enterprise-edition/downloads/index-092322.html?ssSourceSiteId=otncn

1.
Oracle Instant Client Downloads:
http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html
注意需同时下载basic库文件和sdk头文件，如instantclient-basiclite-macos.x64-12.1.0.2.0.zip
和instantclient-sdk-macos.x64-12.1.0.2.0.zip

中文地址：
http://www.oracle.com/technetwork/cn/topics/intel-macsoft-102027-zhs.html

2.
C++ Call Interface Programmer's Guide
https://docs.oracle.com/database/122/LNCPP/toc.htm

3.
Exception: Code - 1804, Message - Error while trying to retrieve text for error ORA-01804
解决办法：
export DYLD_LIBRARY_PATH=$EDB/dblib/plugins/oracle/instantclient/lib
参考地址：
https://blog.caseylucas.com/2013/03/03/oracle-sqlplus-and-instant-client-on-mac-osx-without-dyld_library_path/

4.
