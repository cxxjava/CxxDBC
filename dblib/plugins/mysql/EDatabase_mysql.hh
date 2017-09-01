/*
 * EDatabase_mysql.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef __EDATABASE_MYSQL_HH__
#define __EDATABASE_MYSQL_HH__

extern "C" {
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "./connector/include/mysql.h"
}

#include "../../inc/EDatabase.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

/**
 *
 */

class EDatabase_mysql: public EDatabase {
public:
	virtual ~EDatabase_mysql();

	EDatabase_mysql(ELogger* workLogger, ELogger* sqlLogger,
			const char* clientIP, const char* version);
private:
	MYSQL mysql;
	MYSQL *m_Conn;
	MYSQL_STMT *m_Stmt;
	class Result;
	class Stmt: public EObject {
	public:
		MYSQL_STMT *stmt;
		MYSQL_BIND *fields;
		unsigned long *lengths;
		EA<EA<byte>*> bufs;
		Stmt(Result* result, MYSQL_STMT* stmt, int cols);
		~Stmt();
	};
	class Result: public EObject {
	public:
		int current_row;
		int fetch_size;
		int row_offset;
		int rows_size;
	public:
		~Result();
		Result(MYSQL_RES* result, MYSQL_STMT* stmt, int fetchsize, int offset, int rows);
		MYSQL_RES* getResult();
		Stmt* getStatement();
		void clear();
	public:
		struct Field {
			EString name;
			enum enum_field_types navite_type;
			int length;
			int precision;
			int scale;
			edb_field_type_e type;
		};
		int getCols();
		Field* getField(int col);
	private:
		MYSQL_RES *result;
		Stmt *mystmt;
		Field* fields;
		int cols;
	};
	sp<Result> m_Result;
	
private:
	virtual sp<EBson> onExecute(EBson *req);
	virtual sp<EBson> onUpdate(EBson *req);
	virtual sp<EBson> onMoreResult(EBson *req);
	virtual sp<EBson> onResultFetch(EBson *req);
	virtual sp<EBson> onResultClose(EBson *req);
	virtual sp<EBson> setAutoCommit(boolean autoCommit);
	virtual sp<EBson> onCommit();
	virtual sp<EBson> onRollback();
	virtual sp<EBson> onLOBCreate();
	virtual sp<EBson> onLOBWrite(llong oid, EInputStream *is);
	virtual sp<EBson> onLOBRead(llong oid, EOutputStream *os);

	virtual EString dbtype();
	virtual EString dbversion();
	virtual boolean open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset,
			int timeout);
	virtual boolean close();

	void resetStatement();
	boolean createDataset(EBson *rep, int fetchsize, int offset, int rows,
			boolean newResult = false);

	static edb_field_type_e CnvtNativeToStd(enum enum_field_types type,
            unsigned int length,
            int &precision,
            unsigned int decimals,
            unsigned int flags);
};

} /* namespace edb */
} /* namespace efc */
#endif /* __EDATABASE_MYSQL_HH__ */
