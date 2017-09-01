/*
 * EDBHandler.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "../inc/EDBHandler.hh"
#include "../inc/EConnection.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

extern "C" {
extern efc::edb::EDatabase* makeDatabase(ELogger* workLogger,
		ELogger* sqlLogger, const char* clientIP, const char* version);
typedef efc::edb::EDatabase* (make_database_t)(ELogger* workLogger,
		ELogger* sqlLogger, const char* clientIP, const char* version);
}

static sp<EBson> S2R(EBson* req, sp<ESocket> socket) {
	//1. req
	{
		EByteBuffer buf;
		req->Export(&buf, null, false);
		EOutputStream *os = socket->getOutputStream();
		os->write(EString::formatOf("POST / HTTP/1.1\r\nContent-Length: %d\r\n\r\n", buf.size()).c_str());
		os->write(buf.data(), buf.size());
	}

	//2. rep
	{
		sp<EBson> rep;
		EInputStream *is = socket->getInputStream();
		char s[4096];
		int n;
		EByteBuffer cache(8192, 4096);

		boolean head_found = false;
		int hlen, blen, httplen;
		httplen = hlen = blen = 0;

		while ((n = is->read(s, sizeof(s))) > 0) {
			cache.append(s, n);

			if (!head_found) {
				char* prnrn = eso_strnstr((char*)cache.data(), cache.size(), "\r\n\r\n");
				if (prnrn > 0) {
					head_found = true;
					hlen = prnrn + 4 - (char*)cache.data();

					char* plen = eso_strncasestr((char*)cache.data(), hlen, "Content-Length:");
					if (plen) {
						plen += 15;
						char* plen2 = eso_strstr(plen, "\r\n");
						EString lenstr(plen, 0, plen2-plen);
						blen = EInteger::parseInt(lenstr.trim().c_str());

						httplen = hlen + blen;
					} else {
						httplen = hlen;
					}
				}
			}

			if (head_found && (cache.size() >= httplen)) {
				rep = new EBson();
				rep->Import((char*)cache.data() + hlen, blen);
				break;
			}
		}

		// read error
		if (rep == null) {
			rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
			rep->add(EDB_KEY_ERRMSG, "socket error");
		}

		return rep;
	}
}

//=============================================================================

EDBHandler::~EDBHandler() {
	if (socket != null) {
		socket->close();
	}
}

EDBHandler::EDBHandler(EConnection *conn) {
	m_Conn = conn;
}

void EDBHandler::genRequestBase(EBson *req, int type, boolean needPwd) {
	llong timestamp = ESystem::currentTimeMillis();
	req->addInt(EDB_KEY_MSGTYPE, type);
	if (needPwd) {
		req->add(EDB_KEY_USERNAME, m_Username.c_str());
		if (m_Conn->isProxyMode()) { //代理模式
			EString sb(m_Username);
			sb.append(m_Password).append(timestamp);
			byte pw[ES_MD5_DIGEST_LEN] = {0};
			es_md5_ctx_t c;
			eso_md5_init(&c);
			eso_md5_update(&c, (const unsigned char *)sb.c_str(), sb.length());
			eso_md5_final((es_uint8_t*)pw, &c);
			req->add(EDB_KEY_PASSWORD, EString::toHexString(pw, ES_MD5_DIGEST_LEN).c_str());
		} else {
			req->add(EDB_KEY_PASSWORD, m_Password.c_str());
		}
	}
	req->add(EDB_KEY_VERSION, EDB_INF_VERSION);
	req->addLLong(EDB_KEY_TIMESTAMP, timestamp);
}

void EDBHandler::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset,
		int timeout) {
	m_Username = username;
	m_Password = password;

#if EDB_CLIENT_STATIC // 直连模式(静态编译)
	this->database = makeDatabase(null, null, "localhost", null);
#else
	if (!m_Conn->isProxyMode()) { // 直连模式(动态加载)
		es_string_t* path = NULL;
		eso_current_workpath(&path);
		EString dsopath(path);
		eso_mfree(path);
		dsopath.append("/dblib/")
#ifdef WIN32
				.append("/win/")
				.append(m_Conn->getConnectionDatabaseType())
				.append(".dll");
#else
#ifdef __APPLE__
				.append("/osx/")
#else //__linux__
				.append("/linux/")
#endif
				.append(m_Conn->getConnectionDatabaseType())
				.append(".so");
#endif
		es_dso_t* handle = eso_dso_load(dsopath.c_str());
		if (!handle) {
			throw EFileNotFoundException(__FILE__, __LINE__, dsopath.c_str());
		}
		make_database_t* func = (make_database_t*)eso_dso_sym(handle, "makeDatabase");
		if (!func) {
			throw ERuntimeException(__FILE__, __LINE__, "makeDatabase");
		}
		this->database = func(null, null, "localhost", null);
	} else { // 代理模式
		if (m_Conn->getSSL()) {
			this->socket = new ESSLSocket();
		} else {
			this->socket = new ESocket();
		}
		this->socket->connect(host, port, timeout);
	}
#endif

	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_DBOPEN, true);
	req.add(EDB_KEY_DATABASE, database);
	req.add(EDB_KEY_HOST, host);
	req.addInt(EDB_KEY_PORT, port);
	req.add(EDB_KEY_CHARSET, charset);
	req.addInt(EDB_KEY_TIMEOUT, timeout);

	if (!m_Conn->isProxyMode()) {
		rep = this->database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	// set some connection metadata info.
	m_Conn->getMetaData()->productName = rep->get(EDB_KEY_DBTYPE);
	m_Conn->getMetaData()->productVersion = rep->get(EDB_KEY_DBVERSION);
}

void EDBHandler::close() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_DBCLOSE);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
		database.reset();
	} else {
		rep = S2R(&req, socket);
		socket->close();
	}

	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

boolean EDBHandler::isClosed() {
	if (!m_Conn->isProxyMode()) {
		return !database ? true : false;
	} else {
		return socket->isClosed() ? true : false;
	}
}

sp<EBson> EDBHandler::executeSQL(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket);
	}
}

sp<EBson> EDBHandler::updateSQL(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket);
	}
}

sp<EBson> EDBHandler::moreResult(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket);
	}
}

sp<EBson> EDBHandler::resultFetch(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket);
	}
}

sp<EBson> EDBHandler::resultClose(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket);
	}
}

void EDBHandler::commit() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_COMMIT);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::rollback() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_ROLLBACK);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::setAutoCommit(boolean autoCommit) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_SET_AUTOCOMMIT);

	req.addByte(EDB_KEY_AUTOCOMMIT, autoCommit ? 1 : 0);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::setSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_SETSAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::rollbackSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_BACKSAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::releaseSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_RELESAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

llong EDBHandler::createLargeObject() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_CREATE, true);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_OID);
}

llong EDBHandler::writeLargeObject(llong oid, EInputStream* is) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_WRITE, true);

	req.addLLong(EDB_KEY_OID, oid);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, is);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_WRITTEN);
}

llong EDBHandler::readLargeObject(llong oid, EOutputStream* os) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_READ, true);

	req.addLLong(EDB_KEY_OID, oid);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, os);
	} else {
		rep = S2R(&req, socket);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_READING);
}

} /* namespace edb */
} /* namespace efc */
