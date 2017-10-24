/*
 * EDatabase_freetds.cpp
 *
 *  Created on: 2017-8-10
 *      Author: cxxjava@163.com
 */

#ifndef __EDATABASE_FREETDS_HH__
#define __EDATABASE_FREETDS_HH__

/*
** Include the Client-Library header file.
*/
extern "C" {
#include "./freetds/include/config.h"
#include "./freetds/include/ctpublic.h"
#include "./freetds/include/bkpublic.h"
}

#include "../../inc/EDatabase.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

/**
 *
 */

class EDatabase_freetds: public EDatabase {
public:
	virtual ~EDatabase_freetds();

	EDatabase_freetds(EDBProxyInf* proxy);
private:
	CS_CONTEXT *m_Ctx;
	CS_CONNECTION *m_Conn;
	CS_COMMAND *m_Cmd;

	class Result: public EObject {
	public:
		int current_row;
		int fetch_size;
		int row_offset;
		int rows_size;
	public:
		~Result();
		Result(CS_COMMAND* cmd, int fetchsize, int offset, int rows);
		void clear();
	public:
		struct Field {
			EString name;
			CS_DATAFMT datafmt;
			int navite_type;
			int length;
			int precision;
			int scale;
			edb_field_type_e type;
		};
		int getCols();
		Field* getField(int col);
	private:
		CS_COMMAND* cmd;
		Field* fields;
		int cols;
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
	virtual sp<EBson> setSavepoint(EBson *req);
	virtual sp<EBson> rollbackSavepoint(EBson *req);
	virtual sp<EBson> releaseSavepoint(EBson *req);
	virtual sp<EBson> onLOBCreate();
	virtual sp<EBson> onLOBWrite(llong oid, EInputStream *is);
	virtual sp<EBson> onLOBRead(llong oid, EOutputStream *os);

	virtual EString dbtype();
	virtual EString dbversion();
	virtual boolean open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset,
			int timeout);
	virtual boolean close();

	int getRowsAffected();
	void setErrorMessageDiag(const char *pre=NULL);
	boolean createDataset(EBson *rep, int fetchsize, int offset, int rows,
			boolean newResult = false);

	static edb_field_type_e CnvtNativeToStd(CS_INT type, CS_INT length,
			CS_INT scale, CS_INT precision);
	static CS_INT CnvtStdToNative(edb_field_type_e type);
};

} /* namespace edb */
} /* namespace efc */
#endif /* __EDATABASE_FREETDS_HH__ */
