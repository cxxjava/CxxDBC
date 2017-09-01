http://felix-lin.com/linux/%E4%BD%BF%E7%94%A8-freetds-%E5%AD%98%E5%8F%96-sql-server/

FreeTDS
FreeTDS是一個 Linux 函式庫，他重新實作了 TDS(Tabular Data Stream, 表列資料串流?) 協定，
讓在 Linux 平台運行的程式也可以透過此函式庫存取支援 TDS 的 Sybase SQL 或是 MS SQL Server 資料庫

TDS
TDS 是一個應用層(Application layer)協定，最初由 Sybase 開發，後來在微軟與 Sybase 終止合作關係後便各自發展。
也因此兩者之間的 TDS 雖然基於相同的基礎，但後續實作有所不同，細節可以參考 Sybase(http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.infocenter.dc32600.1570/html/dblib/title.htm) 
與微軟(http://technet.microsoft.com/en-us/library/aa936985(v=sql.80).aspx)的線上手冊。
而重新實作 TDS 的 FreeTDS 也並非支援所有 API，其支援 API 可參考 User Guide(http://freetds.schemamania.org/userguide/dblib.api.summary.htm).


0.
http://www.freetds.org/userguide/choosingtdsprotocol.htm
https://stackoverflow.com/questions/6973371/freetds-problem-connecting-to-sql-server-on-mac-unexpected-eof-from-the-server

TDS version need to match the correct tds protocol to connect to your db server, see below -

http://www.freetds.org/userguide/choosingtdsprotocol.htm

Choosing a TDS protocol version

***DB SERVER        |    TDS VERSION ***    
Microsoft SQL Server 6.x    = 4.2       
Sybase System 10 and above  = 5.0       
Sybase System SQL Anywhere  = 5.0     
Microsoft SQL Server 7.0    = 7.0       
Microsoft SQL Server 2000   = 7.1       
Microsoft SQL Server 2005   = 7.2   
Microsoft SQL Server 2008   = 7.2 
Microsoft SQL Server 2008	7.3
Microsoft SQL Server 2012 or 2014	7.4
N/A	8.0
