/*
 * oracle.cpp
 *
 */

#include "Edb.hh"

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

#define HOST "192.168.2.61"
#define PORT "1521"
#define DATABASE "xe"
#define USERNAME "system"
#define PASSWORD "oracle"

static void test_db_connect() {
	EConnection conn;

	//1.
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);
	ON_FINALLY_NOTHROW(
		conn.close();
	) {
		EDatabaseMetaData* meta = conn.getMetaData();
		LOG("name=%s", meta->getDatabaseProductName().c_str());
		LOG("version=%s", meta->getDatabaseProductVersion().c_str());
	}}

	//2.
	conn.connect("edbc:ORACLE://" HOST ":" PORT "/" DATABASE "?connectTimeout=10", USERNAME, PASSWORD);
	ON_FINALLY_NOTHROW(
		conn.close();
	) {
		EDatabaseMetaData* meta = conn.getMetaData();
		LOG("name=%s", meta->getDatabaseProductName().c_str());
		LOG("version=%s", meta->getDatabaseProductVersion().c_str());
	}}
}

static void test_db_execute() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	EResultSet* rs;

	//0.
	try {
		stmt.setSql("DROP TABLE oracle000").execute();
	} catch (...) {
	}
	stmt.setSql("CREATE TABLE oracle000 ("
            "a int NULL ,"
            "b smallint NULL ,"
            "c char (255) NULL ,"
            "d decimal(12) NULL ,"
            "e clob NULL,"
            "f date NULL ,"
            "g float NULL ,"
            "h number (10,5) NULL ,"
            "i varchar (255) NULL ,"
            "j number(15) NULL ,"
            "k integer NULL ,"
            "l blob NULL)").execute();

	//1.
	stmt.clear();
	stmt.setSql("insert into oracle000 (a,b,c) values(?,?,?)")
				.bindInt(4)
				.bindInt(1)
				.bindString("2017-07-08");
	for (int i=0; i<100; i++) {
		stmt.execute();
	}

	//2.
	stmt.clear();
	stmt.setSql("select * from oracle000").execute();
	rs = stmt.getResultSet();
	EResultSetMetaData* rsm = rs->getMetaData();
	LOG(rsm->toString().c_str());
	while (rs->next()) {
		LOG("%d", rs->getInt(1));
		LOG("%s", rs->getBoolean(2) ? "true" : "false");
		LOG("[%s]", rs->getString(2).c_str());
		LOG("%s", rs->getDate("f").toString("%Y-%m-%d").c_str());
	}
	rs->close();

	//3.
	stmt.clear();
	stmt.setFetchSize(2);
	stmt.setSql("select * from oracle000 where a=? or b=?")
			.bindInt(4)
			.bindInt(1)
			.execute();
	rs = stmt.getResultSet();
	while (rs != null) {
		while (rs->next()) {
			LOG("%d", rs->getInt(1));
			LOG("%s", rs->getBoolean(2) ? "true" : "false");
			LOG("[%s]", rs->getString(2).c_str());
			LOG("%s", rs->getDate("f").toString("%Y-%m-%d").c_str());
		}

		rs = stmt.getResultSet(true);
	}
	if (rs != null) rs->close();

	conn.close();
}

static void test_db_update() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.setFailedResume(true);

	stmt.setSql("insert into oracle000 values (-2, '666', '2015-5-5')"); //1
	stmt.addBatch("insert into oracle000 values (?, ?, ?)"); //2
	stmt.bindInt(-1);
	stmt.bindString("777");
	stmt.bindString("2016-6-6");
	stmt.addBatch("insert into oracle000 values (?, ?, ?)"); //3
		stmt.bindInt(-3);
		stmt.bindString("333");
		stmt.bindString("2016-6-6");
	for (int i = 0; i < 99; i++) {
		stmt.addBatch();
		if (i == 0) { //模拟错误
			stmt.bindInt(i);
		} else {
			stmt.bindInt(i);
			stmt.bindString("999");
			stmt.bindString("2016-6-6");
		}
	}
	stmt.addBatch("insert into oracle000 values (-2, '666', '2015-5-5')"); //103
	stmt.addBatch("insert into oracle000 values (-2, '666', '2015-5-5')"); //104
	stmt.execute();

	LOG("failures=%d", stmt.getFailures());
	LOG("first failed=%d", stmt.getFirstFailed());

	LOG("sql 1 affected=%d", stmt.getUpdateCount(1));
	LOG("sql 3 affected=%d", stmt.getUpdateCount(3));
	LOG("sql 3 errmsg=%s", stmt.getUpdateMessage(3).c_str());
	LOG("sql 4 affected=%d", stmt.getUpdateCount(4));
	LOG("sql 4 errmsg=%s", stmt.getUpdateMessage(4).c_str());
	LOG("sql 5 affected=%d", stmt.getUpdateCount(5));
	LOG("sql 6 affected=%d", stmt.getUpdateCount(6));
	LOG("sql 103 affected=%d", stmt.getUpdateCount(103));
	LOG("sql 104 affected=%d", stmt.getUpdateCount(104));

	conn.close();
}

static void test_db_commit() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	stmt.setSql("update oracle000 set name='xxxxxxxxxx' where id=1");
	conn.setAutoCommit(false);
	stmt.execute();
//	conn.commit();
	conn.rollback();
//	conn.setAutoCommit(true);
	stmt.execute();
	conn.commit();

	conn.close();
}

static void test_db_rollback() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	try {
		stmt.setSql("drop table oracle002").execute();
	} catch (...) {
	}
	stmt.setSql("create table oracle002 (a int)").execute();
	conn.setAutoCommit(false);
	ESavepoint sp1 = conn.setSavepoint();
	stmt.setSql("insert into oracle002 values (1)").execute();
//	conn.rollback();
//	conn.setAutoCommit(true);
	ESavepoint sp2 = conn.setSavepoint();
	stmt.setSql("insert into oracle002 values (2)").execute();
	conn.rollback(sp2);
	conn.commit();

	conn.close();
}

static void test_large_object() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	conn.setAutoCommit(false);

	llong oid = conn.createLargeObject();
	LOG("create oid=%ld", oid);

	char *s = "12345678905567894598765679";
//	EByteArrayInputStream bais(s, strlen(s));
	EByteArrayOutputStream baos;
	for (int i = 0; i< 10000000; i++) {
		baos.write(s);
	}
	EByteArrayInputStream bais(baos.data(), baos.size());
	llong written = conn.writeLargeObject(oid, &bais);
	LOG("written=%ld", written);

	EFileOutputStream fos("/tmp/lob.txt");
	llong reading = conn.readLargeObject(oid, &fos);
	LOG("reading=%ld", reading);

	conn.commit();

	conn.close();
}

static void test_create_table() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.setFailedResume(true);
	stmt.addBatch("drop table oracle001");
	stmt.addBatch("CREATE TABLE oracle001 ("
            "a int NULL ,"
            "b smallint NULL ,"
            "c char (255) NULL ,"
            "d decimal(12) NULL ,"
            "e clob NULL,"
            "f date NULL ,"
            "g float NULL ,"
            "h number (10,5) NULL ,"
            "i varchar (255) NULL ,"
            "j number(15) NULL ,"
            "k integer NULL ,"
            "n binary_double NULL ,"
    		"l blob NULL )");
	stmt.execute();

	conn.close();
}

static void test_sql_insert() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.addBatch("insert into oracle001 (a,b,c,d,e,f,i) values (100000,20,'111212121',1,'中午的太阳火辣辣','2001-01-09 23:12:59','舞起来')");
	stmt.addBatch("insert into oracle001 (a,b,c,d,e,f,i) values (100000,20,'111212121',1,'中午的太阳火辣辣','1999-12-09','舞起来')");
	stmt.addBatch("insert into oracle001 (e,g) values (?,?)");
	es_buffer_t *buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 100; i++) {
		eso_buffer_append(buffer, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);
	stmt.bindFloat(5.44454);
	stmt.addBatch("insert into oracle001 (a,b,c,f,d,i,e) values (?,?,?,?,?,?,?)");
	stmt.bindInt(55555);
	stmt.bindShort(12);
	stmt.bindString("中午的太阳火辣辣");
	stmt.bindDateTime("2005-01-09 23:12:59");
	stmt.bindLong(22220319999);
	stmt.bindString("2222031");
	buffer = eso_buffer_make(100, 0);
	const char* s = "火热反对亲热开防空洞阿若热认可分阿卡人家阿可如风卡人阿克为人咖啡额热炕日方可完全热炕";
	for (int i = 0; i < 100; i++) {
		eso_buffer_append(buffer, s,
			strlen(s));
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);

	stmt.setFailedResume(true);
	stmt.execute();
	LOG("err=%s, first failed sql index=%d, failures=%d", stmt.getErrorMessage().c_str(), stmt.getFirstFailed(), stmt.getFailures());
	for (int i=1; i <= stmt.getSqlCount(); i++) {
		LOG("sql#%d affected=%d, errmsg=%s", i, stmt.getUpdateCount(i), stmt.getUpdateMessage(i).c_str());
	}

	conn.close();
}

static void test_sql_query() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	stmt.setSql("select * from oracle001").execute();
	EResultSet* rs = stmt.getResultSet();
	while (rs->next()) {
		for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
			LOG("%s:%s", rs->getMetaData()->getColumnLabel(i).c_str(), rs->isNull(i) ? "is null" : rs->getString(i).c_str());
		}
	}

	conn.close();
}

static void test_sql_update() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);

	//1.
	stmt.setSql("update oracle001 set a=?,b=?,g=?,n=?,h=?");
	stmt.bindInt(EInteger::MAX_VALUE);
	stmt.bindShort(EShort::MAX_VALUE);
	stmt.bindFloat(8.8843);
	stmt.bindDouble(88888.8843);
	stmt.bindNumeric("55555.5454");
	stmt.execute();

	//2. text: <=64k
	stmt.addBatch("update oracle001 set e=?");
	es_buffer_t *buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 1000; i++) {
		eso_buffer_append(buffer, "a中午的太aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);

	//3. image: <=64k
	stmt.addBatch("update oracle001 set l=?");
	buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 10; i++) {
		eso_buffer_append(buffer, "\0\1\2\aaaaaaaaa中午的太aaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtBLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);

	//4. date
	stmt.addBatch("update oracle001 set f=?");
	stmt.bindDateTime("2005-01-09 23:12:59");

	stmt.execute();

	LOG("sql 1 effected = %d", stmt.getUpdateCount(1));
	LOG("sql 2 effected = %d", stmt.getUpdateCount(2));
	LOG("sql 3 effected = %d", stmt.getUpdateCount(3));
	LOG("sql 4 effected = %d", stmt.getUpdateCount(4));

	conn.close();
}

static void test_sql_func() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//create
	{
		EUpdateStatement stmt(&conn);
		stmt.setFailedResume(true);
		stmt.addBatch("drop procedure oracle001proc");
		stmt.addBatch("CREATE OR REPLACE package pkg_ora001 as"
				" type star_info_ref is ref cursor;"
				" end pkg_ora001;");
		stmt.addBatch("CREATE OR REPLACE PROCEDURE oracle001proc(\n"
                "para1 in varchar2,\n"
                "para2 in number,\n"
                "cur1 out pkg_ora001.star_info_ref,\n"
                "cur2 out pkg_ora001.star_info_ref) AS\n"
                " BEGIN\n"
                "    open cur1 for select * from oracle001 where i=para1;\n"
                "    open cur2 for select * from oracle001 where a=para2;\n"
                "    EXCEPTION\n"
                "      WHEN NO_DATA_FOUND THEN\n"
                "        NULL;\n"
                "      WHEN OTHERS THEN\n"
                "        -- Consider logging the error and then re-raise\n"
                "        RAISE;\n"
                " END oracle001proc;\n");
		stmt.execute();
	}

	//select
	{
		ECommonStatement stmt(&conn);
		stmt.setSql("begin oracle001proc(?,?,?,?); end;");
		stmt.bindString("舞起来");
		stmt.bindInt(55555);
		stmt.registerOutCursor();
		stmt.registerOutCursor();
		stmt.execute();
		EResultSet* rs = stmt.getResultSet();
		while (rs != null) {
			while (rs->next()) {
				for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
					LOG("%s:%s", rs->getMetaData()->getColumnLabel(i).c_str(), rs->isNull(i) ? "is null" : rs->getString(i).c_str());
				}
			}
			rs = stmt.getResultSet(true);
		}
		if (rs != null) rs->close();
	}

	conn.close();
}

static void test_connect_pool() {
	EConnectionPool pool(10, NULL, DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EArrayList<sp<EThread> > threads;
	for (int i = 0; i < 20; i++) {
		sp<EThread> ths = EThread::executeX([&]() {
			sp<EConnection> conn;
			for (int j = 0; j < 1000; j++) {
				conn = pool.getConnection();
				ES_ASSERT (conn != null);
				sp<ECommonStatement> stmt = conn->createCommonStatement();
				stmt->setSql("update oracle002 set a=1").execute();

				EThread::sleep(100);

				conn->close();
			}
		});
		threads.add(ths);
	}

	for (int i=0; i<threads.size(); i++) {
		threads[i]->join();
	}

	pool.close();
}

//=============================================================================

void test_db_oracle(void)
{
	EConnection::setDefaultDBType("ORACLE");

//	test_db_connect();
//	test_db_execute();
//	test_db_update();
//	test_db_commit();
	test_db_rollback();
////	test_large_object();
//	test_create_table();
//	test_sql_insert();
//	test_sql_query();
//	test_sql_update();
//	test_sql_func();
//	test_connect_pool();
}
