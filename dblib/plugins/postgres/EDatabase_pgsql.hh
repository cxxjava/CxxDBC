/*
 * EDatabase_pgsql.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef __EDATABASE_PGSQL_HH__
#define __EDATABASE_PGSQL_HH__

extern "C" {
#include "./libpq/libpq-fe.h"
}

#include "../../inc/EDatabase.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

/**
 *
 */

class EDatabase_pgsql: public EDatabase {
public:
	virtual ~EDatabase_pgsql();

	EDatabase_pgsql(ELogger* workLogger, ELogger* sqlLogger,
			const char* clientIP, const char* version);
private:
	PGconn *m_Conn;
	class Result: public EObject {
	public:
		int current_row;
		int fetch_size;
		int row_offset;
		int rows_size;
	public:
		~Result();
		Result(PGresult* result, int fetchsize, int offset, int rows);
		PGresult* getResult();
		void clear();
	public:
		struct Field {
			EString name;
			Oid navite_type;
			int length;
			int precision;
			int scale;
			edb_field_type_e type;
		};
		int getRows();
		int getCols();
		Field* getField(int col);
	private:
		PGresult *pgResult;
		Field* fields;
		int cols;
		int rows;
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

	void createDataset(EBson *rep, int fetchsize, int offset, int rows,
			boolean newResult = false);
	int alterSQLPlaceHolder(const char *sql_com, EString& sql_pqsql);

	static edb_field_type_e CnvtNativeToStd(Oid type, int length, int precision);
	static Oid CnvtStdToNative(edb_field_type_e eDataType);
};

} /* namespace edb */
} /* namespace efc */
#endif /* __EDATABASE_PGSQL_HH__ */
