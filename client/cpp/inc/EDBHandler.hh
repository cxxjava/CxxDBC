/*
 * EDBHandler.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef EDBHANDLER_HH_
#define EDBHANDLER_HH_

#include "Efc.hh"
#include "../config.hh"
#include "../inc/ESQLException.hh"
#include "../../../dblib/inc/EDatabase.hh"

namespace efc {
namespace edb {

/**
 *
 */

class EConnection;

class EDBHandler: public EObject {
public:
	virtual ~EDBHandler();
	EDBHandler(EConnection *conn);

	void open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset,
			int timeout) THROWS(ESQLException);
	void close() THROWS(ESQLException);

	boolean isClosed();

	void genRequestBase(EBson *req, int type, boolean needPwd=false);

	sp<EBson> executeSQL(EBson *req) THROWS(ESQLException);
	sp<EBson> updateSQL(EBson *req) THROWS(ESQLException);
	sp<EBson> moreResult(EBson *req) THROWS(ESQLException);
	sp<EBson> resultFetch(EBson *req) THROWS(ESQLException);
	sp<EBson> resultClose(EBson *req) THROWS(ESQLException);
	void commit() THROWS(ESQLException);
	void rollback() THROWS(ESQLException);
	void setAutoCommit(boolean autoCommit) THROWS(ESQLException);
	void setSavepoint(const char* name) THROWS(ESQLException);
	void rollbackSavepoint(const char* name) THROWS(ESQLException);
	void releaseSavepoint(const char* name) THROWS(ESQLException);
	llong createLargeObject() THROWS(ESQLException);
	llong writeLargeObject(llong oid, EInputStream* is) THROWS(ESQLException);
	llong readLargeObject(llong oid, EOutputStream* os) THROWS(ESQLException);

private:
	EConnection* m_Conn;
	EString m_Username;
	EString m_Password;

	sp<EDatabase> database;
	sp<ESocket> socket;
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDBHANDLER_HH_ */
