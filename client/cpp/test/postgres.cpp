/*
 * postgres.cpp
 *
 */

#include "Edb.hh"

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

#define HOST "localhost"
#define PORT "6633"
#define DATABASE "postgres"
#define USERNAME "postgres"
#define PASSWORD "password"

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
	conn.connect("edbc:PGSQL://" HOST ":" PORT "/" DATABASE "?connectTimeout=10", USERNAME, PASSWORD);
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
		stmt.setSql("DROP TABLE pqsql000").execute();
	} catch (...) {
	}
	stmt.setSql("CREATE TABLE pqsql000 ("
				  "id integer NULL,"
				  "name varchar (40) NULL ,"
				  "date date NULL"
				  ")").execute();

	//1.
	stmt.clear();
	stmt.setSql("insert into pqsql000 values(?,?,?)")
				.bindInt(4)
				.bindString("1")
				.bindString("2017-07-08");
	for (int i=0; i<100; i++) {
		stmt.execute();
	}

	//2.
	stmt.clear();
	stmt.setSql("select * from pqsql000").execute();
	rs = stmt.getResultSet();
	EResultSetMetaData* rsm = rs->getMetaData();
	LOG(rsm->toString().c_str());
	while (rs->next()) {
		LOG("%d", rs->getInt(1));
		LOG("%s", rs->getBoolean(2) ? "true" : "false");
		LOG("[%s]", rs->getString(2).c_str());
		LOG("%s", rs->getDate("date").toString("%Y-%m-%d").c_str());
	}
	rs->close();

	//3.
	stmt.clear();
	stmt.setFetchSize(2);
	stmt.setSql("select * from pqsql000 where id=? or name=?")
			.bindInt(4)
			.bindString("1")
			.execute();
	rs = stmt.getResultSet();
	while (rs != null) {
		while (rs->next()) {
			LOG("%d", rs->getInt(1));
			LOG("%s", rs->getBoolean(2) ? "true" : "false");
			LOG("[%s]", rs->getString(2).c_str());
			LOG("%s", rs->getDate("date").toString("%Y-%m-%d").c_str());
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

	stmt.setSql("insert into pqsql000 values (-2, '666', '2015-5-5')"); //1
	stmt.addBatch("insert into pqsql000 values (?, ?, ?)"); //2
	stmt.bindInt(-1);
	stmt.bindString("777");
	stmt.bindString("2016-6-6");
	stmt.addBatch("insert into pqsql000 values (?, ?, ?)"); //3
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
	stmt.addBatch("insert into pqsql000 values (-2, '666', '2015-5-5')"); //103
	stmt.addBatch("insert into pqsql000 values (-2, '666', '2015-5-5')"); //104
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
	stmt.setSql("update pqsql000 set name='xxxxxxxxxx' where id=1");
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
		stmt.setSql("drop table pqsql002").execute();
	} catch (...) {
	}
	stmt.setSql("create table pqsql002 (a int)").execute();
	conn.setAutoCommit(false);
	ESavepoint sp1 = conn.setSavepoint();
	stmt.setSql("insert into pqsql002 values (1)").execute();
//	conn.rollback();
//	conn.setAutoCommit(true);
	ESavepoint sp2 = conn.setSavepoint();
	stmt.setSql("insert into pqsql002 values (2)").execute();
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
	stmt.addBatch("drop table pqsql001");
	stmt.addBatch("CREATE TABLE pqsql001 ("
				  "a integer NULL," //int4
				  "b smallint NULL ," //int2
				  "c char (255) NULL ,"
				  "d decimal(12,5) NULL ,"
				  "e text NULL,"
				  "f timestamp NULL ,"
				  "g float NULL ,"
				  "h numeric(10,5) NULL ,"
				  "i varchar (255) NULL ,"
				  "j numeric(15) NULL ,"
				  "k bool ,"
				  "l bytea NULL ,"
				  "m oid ,"
				  "n money ,"
				  "o point ,"
				  "p macaddr ,"
				  "q bit(3) ,"
				  "r serial ,"
			  	  "s bigint" //int8
				  ")");
	stmt.execute();

	conn.close();
}

static void test_sql_insert() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//large lob
	ECommonStatement st(&conn);
	st.setSql("insert into pqsql001 (e,l) values (?,?)");
	EFileInputStream fis1("/tmp/a.csv");
	EFileInputStream fis2("/tmp/b.csv");
	st.bindAsciiStream(&fis1);
	st.bindBinaryStream(&fis2);
	st.execute();

	//other

	EUpdateStatement stmt(&conn);
	stmt.addBatch("insert into pqsql001 (a,b,c,d,e,f,i) values (55555,20,'111212121',1121.3358,'中午的太阳火辣辣','2001-01-09 23:12:59','舞起来')");
	stmt.addBatch("insert into pqsql001 values (100000,20,'111212121',1985.009,'中午的太阳火辣辣','1999-12-09',33.44432,5544.00329,'舞起来',5555444,true," //k
			"'rewrew',787878,9988.87,'(33,44)','08:00:2b:01:02:03',B'101')");
	stmt.addBatch("insert into pqsql001 (e,c) values (?,'?')");
	es_buffer_t *buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 100; i++) {
		eso_buffer_append(buffer, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);
	stmt.addBatch("insert into pqsql001 (a,b,c,f,d,i,l,g) values (?,?,?,?,?,?,?,?)");
	stmt.bindInt(55555);
	stmt.bindShort(12);
	stmt.bindString("12121212121");
	stmt.bindDateTime("2005-01-09 23:12:59");
	stmt.bindNumeric(5555.22);
	stmt.bindString("2222031");
	buffer = eso_buffer_make(100, 0);
	const char * s = "火热反对亲热开防空洞阿若热认可分阿卡人家阿可如风卡人阿克为人咖啡额热炕日方可完全热炕";
	for (int i = 0; i < 100; i++) {
		eso_buffer_append(buffer, s, strlen(s));
	}
	stmt.bind(DB_dtBytes, (char*)buffer->data, buffer->len);
	stmt.bindDouble(8.00432);
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
	stmt.setSql("select * from pqsql001").execute();
	EResultSet* rs = stmt.getResultSet();
	while (rs->next()) {
		for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
			LOG("%s:%s", rs->getMetaData()->getColumnLabel(i).c_str(), rs->isNull(i) ? "is null" : rs->getString(i).c_str());
		}
	}

	conn.close();
}

static void test_sql_cursor() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//游标定义必须在事务环境内
	conn.setAutoCommit(false);

	ECommonStatement stmt(&conn);
	stmt.setSql("DECLARE xxx CURSOR for select * from pqsql001").execute();
	stmt.setSql("FETCH 1000 IN xxx").execute();
	EResultSet* rs = stmt.getResultSet();
	while (rs->next()) {
		LOG(rs->getString(1).c_str());
	}

	conn.close();
}

static void test_sql_update() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.setSql("update pqsql001 set k=?,b=?,a=?,s=?,g=?,h=?");
	stmt.bindBool(1);
	stmt.bindShort(EShort::MAX_VALUE);
	stmt.bindInt(EInteger::MAX_VALUE);
	stmt.bindLong(ELLong::MAX_VALUE);
	stmt.bindFloat(8.8843);
	stmt.bindNumeric("55555.5454");
	stmt.execute();

	conn.close();
}

static void test_sql_func() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//create
	{
		EUpdateStatement stmt(&conn);
		stmt.setFailedResume(true);
		stmt.addBatch("drop function pqsql001func(integer)");
		stmt.addBatch("CREATE OR REPLACE FUNCTION pqsql001func (integer) RETURNS setof pqsql001 AS \n"
					  "$body$ \n"
					  "DECLARE \n"
					  "result record; \n"
					  "BEGIN \n"
					  "  for result in select * from pqsql001 where a=$1 loop \n"
					  "  return next result; \n"
					  "  end loop; \n"
					  "  return; \n"
					  "END; \n"
					  "$body$ \n"
					  "LANGUAGE 'plpgsql' VOLATILE CALLED ON NULL INPUT SECURITY INVOKER;");
		stmt.execute();
	}

	//select
	{
		ECommonStatement stmt(&conn);
		stmt.setSql("select * from pqsql001func(?)");
		stmt.bindInt(55555);
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
				stmt->setSql("update pqsql002 set a=1").execute();

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

void test_db_postgres(void)
{
	EConnection::setDefaultDBType("PGSQL");

//	test_db_execute();
//	test_db_update();
//	test_db_commit();
//	test_db_rollback();
//	test_large_object();
//	test_create_table();
//	test_sql_insert();
	test_sql_insert();
//	test_sql_query();
//	test_sql_cursor();
//	test_sql_update();
//	test_sql_func();
//	test_connect_pool();
}
