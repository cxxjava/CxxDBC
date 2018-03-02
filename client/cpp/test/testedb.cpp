/*
 * testedb.cpp
 *
 *  Created on: 2017-6-14
 *      Author: cxxjava@163.com
 */

#include "es_main.h"
#include "Edb.hh"

#define LOG(fmt,...) ESystem::out->printfln(fmt, ##__VA_ARGS__)

extern void test_db_mysql(void);
extern void test_db_postgres(void);
extern void test_db_oracle(void);
extern void test_db_mssql(void);
extern void test_db_sybase(void);

MAIN_IMPL(testedb) {
	ESystem::init(argc, argv);

	try {
		do {
			LOG("===========begin=============");

			test_db_mysql();
//			test_db_mysql();
//			test_db_oracle();
//			test_db_mssql();
//			test_db_sybase();

//			EThread::sleep(100);

			LOG("===========end=============");
		} while (0);
	}
	catch (EException& e) {
		ESystem::out->printfln("e=%s", e.toString().c_str());
		e.printStackTrace();
	}
	catch (EThrowable& e) {
		e.printStackTrace();
	}
	catch (...) {
		ESystem::out->println("catch all...");
	}

	ESystem::exit(0);

	return 0;
}

