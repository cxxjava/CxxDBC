/*
 * EDatabase_oracle.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef __EDATABASE_ORACLE_HH__
#define __EDATABASE_ORACLE_HH__

#include "../../inc/EDatabase.hh"
#include "../../../interface/EDBInterface.h"

#include "./instantclient/include/occi.h"

using namespace oracle::occi;

namespace efc {
namespace edb {

/**
 *
 */

class EDatabase_oracle: public EDatabase {
public:
	virtual ~EDatabase_oracle();

	EDatabase_oracle(EDBProxyInf* proxy);
private:
	Environment *m_Env;
	Connection *m_Conn;
	Statement  *m_Stmt;
	class Result: public EObject {
	public:
		int current_row;
		int fetch_size;
		int row_offset;
		int rows_size;
	public:
		~Result();
		Result(Statement* stmt, ResultSet* result, int fetchsize, int offset, int rows);
		Result(Statement* stmt, EA<int> cursors, int fetchsize, int offset, int rows);
		ResultSet* getResult();
		void clear();
		ResultSet* next();
	public:
		struct Field {
			EString name;
			int navite_type;
			int length;
			int precision;
			int scale;
			edb_field_type_e type;
		};
		int getCols();
		Field* getField(int col);
	private:
		Statement* stmt;
		ResultSet *result;
		EA<int> cursor_params_index;
		int current_array_index;
		Field* fields;
		int cols;
		void initFields(ResultSet *result);
	};
	sp<Result> m_Result;
	
private:
	virtual sp<EBson> onExecute(EBson *req, EIterable<EInputStream*>* itb);
	virtual sp<EBson> onUpdate(EBson *req, EIterable<EInputStream*>* itb);
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
	int alterSQLPlaceHolder(const char *sql_com, EString& sql_ora);

	static edb_field_type_e CnvtNativeToStd(int type, unsigned int length,
			unsigned int scale, unsigned int precision);
	static int CnvtStdToNative(edb_field_type_e eDataType);
};

} /* namespace edb */
} /* namespace efc */
#endif /* __EDATABASE_ORACLE_HH__ */
