/*
 * EConnection.cpp
 *
 *  Created on: 2017-6-6
 *      Author: cxxjava@163.com
 */

#include "../inc/EConnection.hh"

namespace efc {
namespace edb {

static EString g_default_db_type;

void EConnection::setDefaultDBType(const char* dbtype) {
	g_default_db_type = dbtype;
}

const char* EConnection::getDefaultDBType() {
	return g_default_db_type.c_str();
}

//=============================================================================

EConnection::~EConnection() {
	//
}

EConnection::EConnection(boolean proxy):
		isProxymode(proxy),
		isConnected(false),
		connectTimeout(-1),
		sslActive(false),
		socketTimeout(-1),
		port(DEFAULT_CONNECT_PORT),
		clientEncoding(SQL_DEFAULT_ENCODING),
		autoCommit(true),
		dbHandler(this) {
	if (dbType.isEmpty()) {
		dbType = getDefaultDBType();
	}
}

EConnection::EConnection(const char* dbtype, boolean proxy):
		isProxymode(proxy),
		isConnected(false),
		connectTimeout(-1),
		sslActive(false),
		socketTimeout(-1),
		dbType(dbtype),
		port(DEFAULT_CONNECT_PORT),
		clientEncoding(SQL_DEFAULT_ENCODING),
		autoCommit(true),
		dbHandler(this) {
	if (dbType.isEmpty()) {
		dbType = getDefaultDBType();
	}
}

boolean EConnection::isProxyMode() {
	return isProxymode;
}

void EConnection::setConnectTimeout(int seconds) {
	connectTimeout = seconds;
}

int EConnection::getConnectTimeout() {
	return connectTimeout;
}

void EConnection::setSocketTimeout(int seconds) {
	socketTimeout = seconds;
}

int EConnection::getSocketTimeout() {
	return socketTimeout;
}

void EConnection::setClientEncoding(const char* charset) {
	clientEncoding = charset;
}

EString EConnection::getClientEncoding() {
	return clientEncoding;
}

void EConnection::setSSL(boolean on) {
	sslActive = on;
}

boolean EConnection::getSSL() {
	return sslActive;
}

void EConnection::connect(const char *url) {
	if (isConnected) {
		throw ESQLException(__FILE__, __LINE__, "has connected.");
	}

	this->parseUrl(url);

	connectInternal();
	isConnected = true;
}

void EConnection::connect(const char* url, const char* username,
		const char* password) {
	if (isConnected) {
		throw ESQLException(__FILE__, __LINE__, "has connected.");
	}

	this->parseUrl(url);
	this->username = username;
	this->password = password;

	connectInternal();
	isConnected = true;
}

void EConnection::connect(const char* database, const char* host,
		const int port, const char* username, const char* password) {
	if (isConnected) {
		throw ESQLException(__FILE__, __LINE__, "has connected.");
	}

	this->database = database;
	this->host = host;
	this->port = port;
	this->username = username;
	this->password = password;

	connectInternal();
	isConnected = true;
}

void EConnection::connectInternal() {
	if (dbType.isEmpty()) {
		dbType = getDefaultDBType();
	}

	dbHandler.open(database.c_str(), host.c_str(), port, username.c_str(),
			password.c_str(), clientEncoding.c_str(), connectTimeout);
}

void EConnection::close(void) {
	if (isClosed()) {
		return;
	}
	dbHandler.close();
	isConnected = false;
}

boolean EConnection::isClosed() {
	return dbHandler.isClosed();
}

EString EConnection::getConnectionDatabaseType(void) {
	return dbType;
}

EString EConnection::getConnectionDatabaseName(void) {
	return database;
}

EString EConnection::getConnectionUsername(void) {
	return username;
}

EString EConnection::getConnectionPassword(void) {
	return password;
}

EDatabaseMetaData* EConnection::getMetaData() {
	return &metaData;
}

sp<ECommonStatement> EConnection::createCommonStatement() {
	return new ECommonStatement(this);
}

sp<EUpdateStatement> EConnection::createUpdateStatement() {
	checkClosed();
	return new EUpdateStatement(this);
}

void EConnection::setAutoCommit(boolean autoCommit) {
	checkClosed();
	dbHandler.setAutoCommit(autoCommit);
	this->autoCommit = autoCommit;
}

boolean EConnection::getAutoCommit() {
	return autoCommit;
}

void EConnection::commit(void) {
	checkClosed();
	dbHandler.commit();
}

void EConnection::rollback(void) {
	checkClosed();
	dbHandler.rollback();
}

ESavepoint EConnection::setSavepoint() {
	return setSavepoint(NULL);
}

ESavepoint EConnection::setSavepoint(const char* name) {
	checkClosed();

	if (autoCommit) {
	  throw ESQLException(__FILE__, __LINE__, "Cannot establish a savepoint in auto-commit mode.");
	}

	ESavepoint savepoint(name);
	dbHandler.setSavepoint(savepoint.getSavepointName().c_str());
	return savepoint;
}

void EConnection::rollback(ESavepoint savepoint) {
	checkClosed();
	dbHandler.rollbackSavepoint(savepoint.getSavepointName().c_str());
}

void EConnection::releaseSavepoint(ESavepoint savepoint) {
	checkClosed();
	dbHandler.releaseSavepoint(savepoint.getSavepointName().c_str());
	savepoint.invalidate();
}

void EConnection::checkClosed() {
	if (isClosed()) {
		throw ESQLException(__FILE__, __LINE__, "This connection has been closed.");
	}
}

void EConnection::checkConnected() {
	if (isConnected) {
		throw ESQLException(__FILE__, __LINE__, "This connection has been connected.");
	}
}

void EConnection::parseUrl(const char* url) {
	if (!url || !*url) {
		throw ENullPointerException(__FILE__, __LINE__);
	}

	if (eso_strncmp(url, "edbc:", 5) != 0) {
		throw EProtocolException(__FILE__, __LINE__, "edbc:");
	}

	//edbc:[DBTYPE]://host:port/database?connectTimeout=xxx&socketTimeout=xxx&clientEncoding=xxx

	EURI uri(url + 5 /*edbc:*/);
	dbType = uri.getScheme();
	host = uri.getHost();
	port = uri.getPort();
	database = uri.getPath();
	database = database.substring(1); //index 0 is '/'
	EString ct = uri.getParameter("connectTimeout");
	if (!ct.isEmpty()) {
		connectTimeout = EInteger::parseInt(ct.c_str());
	}
	EString st = uri.getParameter("socketTimeout");
	if (!st.isEmpty()) {
		socketTimeout = EInteger::parseInt(st.c_str());
	}
	EString ce = uri.getParameter("clientEncoding");
	if (!ce.isEmpty()) {
		clientEncoding = ce;
	}

	if (database.isEmpty() || host.isEmpty() || database.isEmpty()) {
		throw EIllegalArgumentException(__FILE__, __LINE__, "url");
	}
	if (port == -1) {
		port = DEFAULT_CONNECT_PORT;
	}
	username = uri.getParameter("username");
	password = uri.getParameter("password");
	sslActive = EBoolean::parseBoolean(uri.getParameter("ssl", "false").c_str());
}

llong EConnection::createLargeObject() {
	return dbHandler.createLargeObject();
}

llong EConnection::writeLargeObject(llong oid, EInputStream* is) {
	return dbHandler.writeLargeObject(oid, is);
}

llong EConnection::readLargeObject(llong oid, EOutputStream* os) {
	return dbHandler.readLargeObject(oid, os);
}

} /* namespace edb */
} /* namespace efc */
