/*
 * ECommonStatement.cpp
 *
 *  Created on: 2017-6-5
 *      Author: cxxjava@163.com
 */

#include "../inc/ECommonStatement.hh"
#include "../inc/EConnection.hh"

namespace efc {
namespace edb {

ECommonStatement::~ECommonStatement() {
}

ECommonStatement::ECommonStatement() :
		updateCount(0) {
}

ECommonStatement::ECommonStatement(EConnection *conn) :
		EStatement(conn), updateCount(0) {
}

ECommonStatement& ECommonStatement::registerOutCursor() {
	bind(DB_dtCursor, null);
	return *this;
}

EStatement& ECommonStatement::execute(void) {
	if (!connection) {
		throw ESQLException(__FILE__, __LINE__, "connection lost");
	}

	if (!hasRequestHead) {
		connection->dbHandler.genRequestBase(&sqlStmt, DB_SQL_EXECUTE);
		hasRequestHead = true;
	}
	sqlStmt.setInt(EDB_KEY_FETCHSIZE, getFetchSize());

	//请求并获取结果
	sp<EBson> rep = connection->dbHandler.executeSQL(&sqlStmt, &streamParams);

	if (!rep || rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->getString(EDB_KEY_ERRMSG).c_str());
	}

	updateCount = rep->getInt(EDB_KEY_AFFECTED, -1);

	if (rep->find(EDB_KEY_DATASET)) {
		resultSet = new EResultSet(this, rep);
	}

	return *this;
}

EResultSet* ECommonStatement::getResultSet(boolean next) {
	if (!next || getMoreResults()) {
		return resultSet.get();
	} else {
		if (resultSet != null) {
			resultSet->close();
		}
		return null;
	}
}

boolean ECommonStatement::getMoreResults() {
	if (!resultSet) {
		return false;
	}

	EBson req;
	connection->dbHandler.genRequestBase(&req, DB_SQL_MORE_RESULT);
	req.addLLong(EDB_KEY_CURSOR, resultSet->cursorID);

	//请求并获取结果
	sp<EBson> rep = connection->dbHandler.moreResult(&req);

	if (!rep || rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, "more result");
	}

	if (rep->find(EDB_KEY_DATASET)) {
		resultSet = new EResultSet(this, rep);
		return true;
	} else {
		resultSet.reset();
		return false;
	}
}

int ECommonStatement::getUpdateCount() {
	if (!resultSet) {
		return -1;
	}
	return updateCount;
}

EString ECommonStatement::getErrorMessage(void) {
	return resultSet->dataset->getString(EDB_KEY_ERRMSG);
}

void ECommonStatement::clear() {
	resultSet.reset();
}

} /* namespace edb */
} /* namespace efc */
