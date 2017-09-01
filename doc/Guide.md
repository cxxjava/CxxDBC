# CxxDBC使用指南

版本：0.1.0  
更新：2017/08/30

## 目录

- [适用对象](#适用对象：)
- [术语定义](#术语定义：)
- [源码目录](#源码目录：)
- [Client概要](#Client概要：)
- [Apps详解](#Apps详解：)
- [编译说明](#编译说明：)
- [其他说明](#其他说明：)
- [FAQ](#FAQ：)

### 适用对象：
本指南适用于具备c++和关系型数据库基础知识的IT人员。

如果您同时了解jdbc的基本知识，那么app集成CxxDBC Client会非常容易；
如果您同时熟悉linux的基础知识，那么app集成CxxDBC dbproxy模式很非常容易(需要部署CxxDBC dbproxy server)；


### 术语定义：
**client**     ：数据库连接客户端开发库，不同的数据库产品使用相同的访问API。
**dbtype ** ：数据库类型，指不同数据库产品的区分关键字，在CxxDBC中，*dbtype*用于*dblib*下各插件动态链接库的加载定位和*client*的目标库连接定位；
**dblib **     ：数据库插件，也可以叫原生数据库*drvier*，不同的数据库产品需要分别封装，在CxxDBC中，通常插件都是通过动态加载实现；
**dbproxy**：数据库代理服务，在CxxDBC中，使用`dbproxy代理访问`模式时，*dbproxy server*起到了关键的作用。


### 源码目录：
	CxxDBC --        #根目录
		|- apps      #基于client/dblib开发的各类应用
			|- esql          #类postgres psql的客户端工具
			|- lite_dbproxy  #简版实现的dbproxy代理服务器程序
		|- client    #集成到宿主app的数据库访问client库
			|- c             #TODO
			|- cpp           #c++版client库
			|- java          #TODO
		|- dblib     #数据库Native Devier封装插件集
			|- plugins
				|- freetds   #Microsoft SQL Server和Sybase的开源访问实现
				|- mysql     #MySQL插件实现
				|- oracle    #Oracle插件实现
				|- postgres  #Postgres插件实现
				|- ...       #TODO
		|- interface #client和dblib间的接口定义


### Client概要：
这里的概要指的是对已实现的c++版CxxDBC Client的API使用进行关键点说明。

* **EConnection**

  `连接方式`：

  1）URL：
  ```
  edbc:[dbtype]://<host:port>/<database>?connectTimeout=<连接超时时间，单位：秒>&socketTimeout=<数据库操作Socket超时时间，单位：秒>&clientEncoding=<指定返回结果数据字符集，默认'utf-8'>&username=<用户名>&password=<密码>&ssl=<true|false，是否ssl安全连接，仅proxy模式时有效>
  ```
  2）函数参数：参见函数定义
  `c++:`

  ```
  virtual void connect(const char *database,
                 const char *host,
                 const int   port,
                 const char *username,
                 const char *password) THROWS(ESQLException);
  ```
  ​

  `访问模式`：

  1）静态链接：可访问数据库类型固定，具体参见`编译说明`一节；

  2）动态直连：*EConnection*对象默认构造使用*dbproxy*模式，具体由构造参数`boolean proxy`决定，当`proxy==false`时，*client*访问数据库使用`动态直连`模式，即根据*dbtype*确定动态加载具体的*dblib*数据库访问插件，如`dbtype=='MYSQL'`时，*client*自动动态加载`MYSQL.so`(WIN下为`MYSQL.dll`)。

  3）代理访问：*client*通过*dbproxy*访问数据库，同时*dbproxy*根据*client*的*dbtype*确定具体连接哪个数据库，原来类同`动态直连`时的动态加载过程。

  ​

* **ECommonStatement**

  通用的数据库访问Statement对象，支持所有的sql语句操作（相当于集合了jdbc的*execute*、*executeQuery*、*PreparedStatement*三类操作，API更精简）

  ​

* **EUpdateStatement**


  仅用于批量更新的sql操作，让sql执行更有效率。

  ​

* **EConnectionPool**

  数据库访问连接池，*EConnection*对象简单的从*EConnectionPool*对象*getConnection()*即可，*EConnection*对象使用结束智能回收。

  ​

### Apps详解：

​	这里的apps指的是已经编译为目标平台(x64)的可执行程序，包括`esql`和`lite_dbproxy`。

* **安装**： 
  ```
  tar zxvf apps/release/cxxdbc_apps_v0.1.0.tar.gz;
  #以下步骤仅Oracle数据库插件需开启的情况下执行：
  cd [目标平台，linux or osx];
  cd dblib;
  tar zxvf oracle.tar.gz;
  mv lib oracle;
  ```

  解压后目录结构如下：

  apps --        #根目录

  	|- certs   #存放SSL证书
  	|- linux   #linux平台程序
  		|- dblib             #数据库插件集合
  		|- dbproxy           #dbproxy代理服务主程序
  		|- dbproxy.ini       #dbproxy代理服务配置文件
  		|- log4e.properties  #dbproxy代理服务日志配置
  		|- esql              #类psql客户端访问程序
  	|- osx     #osx平台程序，子目录结构同linux平台目录


  

* **esql**：类postgres psql的客户端工具，支持`动态直连`和`代理访问`两种模式。

  **执行**：  

  ```
  $ ./esql
  ==============================
  | Name: esql, Version: 0.1.0 |
  | Author: cxxjava@163.com    |
  | https://github.com/cxxjava |
  ==============================
  Usage: ./esql [-c url]
         ./esql [-d] [-v] [?]
  Options:
  -c				 : [-c connect url]
  -d				 : direct connection
  -v				 : show version number
  ?				 : list available command line options
  $
  $ ./esql -c "edbc:ORACLE://localhost:6633/xe?username=oracle&password=password"
  ==============================
  | Name: esql, Version: 0.1.0 |
  | Author: cxxjava@163.com    |
  | https://github.com/cxxjava |
  ==============================
  dbtype:ORACLE, version:Oracle Database 12c Standard Edition Release 12.1.0.2.0 - 64bit Production
  1> select * from oracle001
  2> go
  ```

  注：1) *esql*默认走`dbproxy代理模式`，如需数据库直连访问，则请添加`-d`参数；

  ​        2) *go*命令后回车即表示执行前面输入的*sql*语句。

  ​

* **lite_dbproxy**：数据库代理访问程序，即`代理访问`模式时的后端服务程序。
   **配置**：
   **dbproxy.ini**  
    ```
   #Lite-DBProxy Config File

   [COMMON]
   #最大连接数
   max_connections = 100
   #SO超时时间：秒
   socket_timeout = 3
   #SSL证书设置
   ssl_cert = "../certs/tests-cert.pem"
   ssl_key = "../certs/tests-key.pem"

   [SERVICE]
   #服务端口号
   listen_port = 6633

   [SERVICE]
   #服务端口号
   listen_port = 6643
   #SSL加密
   ssl_active = TRUE

   #[WHITELIST]
   #白名单
   #127.0.0.1

   [BLACKLIST]
   #黑名单
   192.168.0.199

   [DBTYPE]
   #支持的数据库类型: 类型=true(开)|false(关)
   MYSQL=true
   PGSQL=true
   ORACLE=true
   MSSQL=true
   SYBASE=true

   [USERMAP]
   #客户端访问用户列表: 访问用户名=数据库别名,访问密码
   postgres=db_postgres,password
   mysql=db_mysql,password
   oracle=db_oracle,password
   mssql=db_mssql,password
   sybase=db_sybase,password

   [DBLIST]
   #服务数据库列表: 数据库别名=数据库访问URL
   db_postgres = "edbc:PGSQL://localhost:5432/postgres?connectTimeout=30&username=postgres&password=xxx"
   db_mysql = "edbc:MYSQL://localhost:3306/test?connectTimeout=30&username=test&password=xxx"
   db_oracle = "edbc:ORACLE://localhost:1521/xe?connectTimeout=30&username=system&password=xxx"
   db_mssql = "edbc:MSSQL://localhost:1433/master?connectTimeout=30&username=sa&password=xxx"
   db_sybase = "edbc:MSSQL://localhost:5000/master?connectTimeout=30&username=sa&password=xxx"
    ```
   注：1) 当添加`ssl_active = TRUE`的`[SERVICE]`时，`SSL证书设置`为必须；

   ​	2) 若要将`白名单`功能关闭，则需删除或注释掉整段`[WHITELIST]`，否则拒绝所有客户端访问；

   ​	3) 启用某一个数据类型后（即`[DBTYPE]`相应位置为*true*），对`[DBLIST]`下对应的相同*dbtype*链接都将生效；

   ​	4) 在代理访问模式下，CxxDBC Client的访问用户名和密码改为`[USERMAP]`下设置的用户名和密码，而非数据库原始用户名密码；

   ​

   **log4e.properties ** 

    ```
   # Root logger settings
   log4j.rootLogger=TRACE, file
   log.path = /tmp

   # output to file
   log4j.appender.file = org.apache.log4j.RollingFileAppender
   log4j.appender.file.File = ${log.path}/all.log
   log4j.appender.file.BufferedIO = false
   log4j.appender.file.BufferSize = 8192
   log4j.appender.file.MaxFileSize = 5MB
   log4j.appender.file.MaxBackupIndex = 1
   log4j.appender.file.layout = org.apache.log4j.PatternLayout
   log4j.appender.file.layout.ConversionPattern = %-d{yyyy-MM-dd HH:mm:ss}  [ %l:%c:%t:%r ] - [ %p ]  %X{} %m %T %n
    ```

   注：具体配置方法参见java版log4j.properties的配置。

   ​

   **执行**：  
    ```
    $ ./dbproxy
    Usage: dbproxy -c [config file]
          +d : daemon mode
          +c : show cofing sample
          +h : show help
          +v : show version
    $
    $ ./dbproxy -c dbproxy.ini
    virtual database connecting test...
    virtual database [db_oracle] connect success.
    listen port: 6633
    listen port: 6643
    ```

   注：1) 若需daemon模式运行，请添加参数`+d`；

   ​	2) lite_proxy目前仅简版实现，后续更多功能正在开发中...

   ​	

### 编译说明：
1) **client静态链接**：修改`client/cpp/config.hh`内宏开关值为`1`：  

```
/**
 * 功能：edb client 编译模式开关
 * 说明：1-静态链接 | 0-其他模式
 */
#define EDB_CLIENT_STATIC  1
```

同时将client代码和对应数据库的插件代码添加到宿主app工程，例如静态链接*mysql*插件，则app工程需要添加如下文件或目录：

```
client/cpp/config.hh
client/cpp/Edb.hh
client/cpp/src/...
dblib/inc/EDatabase.hh
dblib/src/EDatabase.cpp
dblib/plugins/mysql/EDatabase_mysql.cpp
dblib/plugins/mysql/EDatabase_mysql.hh
dblib/plugins/mysql/connector/
```

注：mysql connector library (dblib/plugins/mysql/connector/lib/libmysqlclient.a)需要预先自行编译。



2) **client其他模式**：修改`client/cpp/config.hh`内宏开关值为`0`：  

```
/**
 * 功能：edb client 编译模式开关
 * 说明：1-静态链接 | 0-其他模式，其他模式即为动态直连和代理访问这两种模式，或两种模式的混合。
 */
#define EDB_CLIENT_STATIC  0
```

同时仅需添加client代码到宿主app工程：

```
client/cpp/config.hh
client/cpp/Edb.hh
client/cpp/src/...
```



### 其他说明：
CxxDBC数据库插件的跨平台特性依据数据库原生client library的跨平台特性而定，但多数情况下都能从网上找到相应操作系统平台下的library，其中：

freetds请参考：http://www.freetds.org/

mysql请参考：https://dev.mysql.com/downloads/connector/c/6.1.html

oracle请参考：http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html

postgres请参考：https://www.postgresql.org/ftp/source/

 	


### FAQ：
TODO.

