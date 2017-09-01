/*
 * EDatabase.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "../inc/EDatabase.hh"
#include "../../interface/EDBInterface.h"

namespace efc {
namespace edb {

EDatabase::~EDatabase() {
	//
}

EDatabase::EDatabase(ELogger* workLogger, ELogger* sqlLogger,
		const char* clientIP, const char* version) :
		m_WorkLogger(workLogger),
		m_SqlLogger(sqlLogger),
		m_ClientIP(clientIP),
		m_ProxyVersion(version),
		m_AutoCommit(true),
		m_ErrorCode(0),
		m_CursorID(0) {
}

sp<EBson> EDatabase::processSQL(EBson *req, void *arg) {
#ifdef DEBUG
	showMessage(req);
#endif

	sp<EBson> rep;
	int ope = req->getInt(EDB_KEY_MSGTYPE);
	switch (ope) {
	case DB_SQL_DBOPEN:
		rep = onOpen(req);
		break;
	case DB_SQL_DBCLOSE:
		rep = onClose(req);
		break;
	case DB_SQL_EXECUTE:
		rep = onExecute(req);
		break;
	case DB_SQL_UPDATE:
		rep = onUpdate(req);
		break;
	case DB_SQL_MORE_RESULT:
		rep = onMoreResult(req);
		break;
	case DB_SQL_RESULT_FETCH:
		rep = onResultFetch(req);
		break;
	case DB_SQL_RESULT_CLOSE:
		rep = onResultClose(req);
		break;
	case DB_SQL_SET_AUTOCOMMIT:
	{
		boolean flag = req->getByte(EDB_KEY_AUTOCOMMIT) ? true : false;
		rep = setAutoCommit(flag);
		break;
	}
	case DB_SQL_COMMIT:
		rep = onCommit();
		break;
	case DB_SQL_ROLLBACK:
		rep = onRollback();
		break;
	case DB_SQL_SETSAVEPOINT:
		rep = setSavepoint(req);
		break;
	case DB_SQL_BACKSAVEPOINT:
		rep = rollbackSavepoint(req);
		break;
	case DB_SQL_RELESAVEPOINT:
		rep = releaseSavepoint(req);
		break;
	case DB_SQL_LOB_CREATE:
		rep = onLOBCreate();
		break;
	case DB_SQL_LOB_WRITE:
	{
		EInputStream *is = (EInputStream*)arg;
		llong oid = req->getLLong(EDB_KEY_OID);
		rep = onLOBWrite(oid, is);
		break;
	}
	case DB_SQL_LOB_READ:
	{
		EOutputStream *os = (EOutputStream*)arg;
		llong oid = req->getLLong(EDB_KEY_OID);
		rep = onLOBRead(oid, os);
		break;
	}
	default:
	{
		m_ErrorCode = -1;
		m_ErrorMessage = EString::formatOf("No #%d message.", ope);
		break;
	}
	}

	if (!rep) {
		rep = genRspCommFailure();
	}

#ifdef DEBUG
	showMessage(rep.get());
#endif

	return rep;
}

sp<EBson> EDatabase::onOpen(EBson *req) {
	char* database = req->get(EDB_KEY_DATABASE);
	char* host = req->get(EDB_KEY_HOST);
	int port = req->getInt(EDB_KEY_PORT);
	char* username = req->get(EDB_KEY_USERNAME);
	char* password = req->get(EDB_KEY_PASSWORD);
	char* charset = req->get(EDB_KEY_CHARSET);
	int timeout = req->getInt(EDB_KEY_TIMEOUT, 60); //默认60秒

	boolean ret = open(database, host, port, username, password, charset, timeout);
	if (!ret) {
		return null;
	}

	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	rep->add(EDB_KEY_VERSION, getProxyVersion());
	rep->add(EDB_KEY_DBTYPE, dbtype().c_str());
	rep->add(EDB_KEY_DBVERSION, dbversion().c_str());
	return rep;
}

sp<EBson> EDatabase::onClose(EBson *req) {
	boolean ret = close();
	if (!ret) {
		return null;
	}

	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	return rep;
}

sp<EBson> EDatabase::doSavepoint(EBson *req, EString& sql) {
	//转换请求为onUpdate(req)
	req->setInt(EDB_KEY_MSGTYPE, DB_SQL_UPDATE);
	req->add(EDB_KEY_SQLS "/" EDB_KEY_SQL, sql.c_str());
	return onUpdate(req);
}

sp<EBson> EDatabase::setSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	EString sql = EString::formatOf("SAVEPOINT %s", name.c_str());
	return doSavepoint(req, sql);
}

sp<EBson> EDatabase::rollbackSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	EString sql = EString::formatOf("ROLLBACK TO SAVEPOINT %s", name.c_str());
	return doSavepoint(req, sql);
}

sp<EBson> EDatabase::releaseSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	EString sql = EString::formatOf("RELEASE SAVEPOINT %s", name.c_str());
	return doSavepoint(req, sql);
}

boolean EDatabase::getAutoCommit() {
	return m_AutoCommit;
}

llong EDatabase::newCursorID() {
	return ++m_CursorID;
}

llong EDatabase::currCursorID() {
	return m_CursorID;
}

sp<EBson> EDatabase::genRspCommSuccess() {
	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	return rep;
}

sp<EBson> EDatabase::genRspCommFailure() {
	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
	rep->add(EDB_KEY_ERRMSG, getErrorMessage());
	return rep;
}

void EDatabase::dumpSQL(const char *oldSql, const char *newSql) {
	if (m_SqlLogger != null && (oldSql || newSql)) {
		m_SqlLogger->log(null, -1, ELogger::LEVEL_INFO, newSql ? newSql : oldSql, null);
	}
}

void EDatabase::setErrorCode(int errcode) {
	m_ErrorCode = errcode;
}

int EDatabase::getErrorCode() {
	return m_ErrorCode;
}

void EDatabase::setErrorMessage(const char* message) {
	m_ErrorMessage = message;
}

const char* EDatabase::getErrorMessage() {
	return m_ErrorMessage.c_str();
}

const char* EDatabase::getProxyVersion() {
	return m_ProxyVersion.c_str();
}

#ifdef DEBUG
void EDatabase::showMessage(EBson* bson) {
	EByteArrayOutputStream baos;
	bson->Export(&baos, NULL);
	EByteArrayInputStream bais(baos.data(), baos.size());
	class BsonParser : public EBsonParser {
	public:
		BsonParser(EInputStream *is) :
				EBsonParser(is) {
		}
		void parsing(es_bson_node_t* node) {
			if (!node) return;

			for (int i=1; i<_bson->levelOf(node); i++) {
				printf("\t");
			}
			printf(node->name);
			printf("-->");
			if (eso_strcmp(node->name, "param") == 0) {
				printf("?");
			} else if (eso_strcmp(node->name, "record") == 0) {
				es_size_t size = 0;
				void *value = EBson::nodeGet(node, &size);
				EByteArrayInputStream bais(value, size);
				EDataInputStream dis(&bais);
				int len;
				try {
					while ((len = dis.readInt()) != -1) {
						EA<byte> buf(len + 1);
						dis.read(buf.address(), len);
						printf(" %s |", buf.address());
					}
				} catch (...) {
				}
			} else {
				printf(EBson::nodeGetString(node).c_str());
			}
			printf("\n");
		}
	};

	BsonParser ep(&bais);
	EBson bson_;
	while (ep.nextBson(&bson_)) {
		//
	}
}
#endif

} /* namespace edb */
} /* namespace efc */
