/*
 * EConnectionPool.cpp
 *
 *  Created on: 2017-7-4
 *      Author: cxxjava@163.com
 */

#include "../inc/EConnectionPool.hh"

namespace efc {
namespace edb {

class PooledConnection: public EConnection {
public:
	PooledConnection(EConnectionPool *pool, sp<EConnection> conn) :
			pool(pool), conn(conn) {
	}
	~PooledConnection() {
		try {
			close();
		} catch (...) {
		}
	}
	virtual void setConnectTimeout(int seconds) {
		checkClosed();
		conn->setConnectTimeout(seconds);
	}
	virtual int getConnectTimeout() {
		checkClosed();
		return conn->getConnectTimeout();
	}
	void setSocketTimeout(int seconds) {
		checkClosed();
		conn->setSocketTimeout(seconds);
	}
	virtual int getSocketTimeout() {
		checkClosed();
		return conn->getSocketTimeout();
	}
	virtual void setClientEncoding(const char* charset) {
		checkClosed();
		conn->setClientEncoding(charset);
	}
	virtual EString getClientEncoding() {
		checkClosed();
		return conn->getClientEncoding();
	}
	virtual void close(void) THROWS(ESQLException) {
		if (conn != null) {
			//恢复默认参数
			conn->setClientEncoding(pool->getDefaultClientEncoding().c_str());
			conn->setConnectTimeout(pool->getDefaultConnectTimeout());
			conn->setSocketTimeout(pool->getDefaultSocketTimeout());

			//加入空闲队列
			pool->reclaim(conn);
			conn = null;
		}
	}
	virtual boolean isClosed() THROWS(ESQLException) {
		return (conn == null);
	}
	virtual EString getConnectionDatabaseType(void) {
		checkClosed();
		return conn->getConnectionDatabaseType();
	}
	virtual EString getConnectionDatabaseName(void) {
		checkClosed();
		return conn->getConnectionDatabaseName();
	}
	virtual EString getConnectionUsername(void) {
		checkClosed();
		return conn->getConnectionUsername();
	}
	virtual EString getConnectionPassword(void) {
		checkClosed();
		return conn->getConnectionPassword();
	}
	virtual EDatabaseMetaData* getMetaData() THROWS(ESQLException) {
		checkClosed();
		return conn->getMetaData();
	}
	virtual sp<ECommonStatement> createCommonStatement() THROWS(ESQLException) {
		checkClosed();
		return conn->createCommonStatement();
	}
	virtual sp<EUpdateStatement> createUpdateStatement() THROWS(ESQLException) {
		checkClosed();
		return conn->createUpdateStatement();
	}
	virtual void setAutoCommit(boolean autoCommit) THROWS(ESQLException) {
		checkClosed();

	}
	virtual boolean getAutoCommit() THROWS(ESQLException) {
		checkClosed();
		return conn->getAutoCommit();
	}
	virtual void commit(void) THROWS(ESQLException) {
		checkClosed();
		conn->commit();
	}
	virtual void rollback(void) THROWS(ESQLException) {
		checkClosed();
		conn->rollback();
	}
	virtual ESavepoint setSavepoint() THROWS(ESQLException) {
		checkClosed();
		return conn->setSavepoint();
	}
	virtual ESavepoint setSavepoint(const char* name) THROWS(ESQLException) {
		checkClosed();
		return conn->setSavepoint(name);
	}
	virtual void rollback(ESavepoint savepoint) THROWS(ESQLException) {
		checkClosed();
		conn->rollback(savepoint);
	}
	virtual void releaseSavepoint(ESavepoint savepoint) THROWS(ESQLException) {
		checkClosed();
		conn->releaseSavepoint(savepoint);
	}
	virtual llong createLargeObject() THROWS(ESQLException) {
		checkClosed();
		return conn->createLargeObject();
	}
	virtual llong writeLargeObject(llong oid, EInputStream* is) THROWS(ESQLException) {
		checkClosed();
		return conn->writeLargeObject(oid, is);
	}
	virtual llong readLargeObject(llong oid, EOutputStream* os) THROWS(ESQLException) {
		checkClosed();
		return conn->readLargeObject(oid, os);
	}
private:
	EConnectionPool *pool;
	sp<EConnection> conn;

	void checkClosed() {
		if (isClosed()) {
			throw ESQLException(__FILE__, __LINE__, "This connection has been closed.");
		}
	}
	virtual void connect(const char *url,
		             const char *username,
		             const char *password) THROWS(ESQLException) {
		//
	}
	virtual void connect(const char *database,
				 const char *host,
				 const int   port,
				 const char *username,
				 const char *password) THROWS(ESQLException) {
		checkClosed();
		return conn->connect(database, host, port, username, password);
	}
};

//=============================================================================

EConnectionPool::EConnectionPool(int maxconns,
			const char *url,
            const char *username,
            const char *password,
            boolean ssl):
    		connectTimeout(-1),
    		socketTimeout(-1),
    		dbType(EConnection::getDefaultDBType()),
    		port(EConnection::DEFAULT_CONNECT_PORT),
    		username(username),
    		password(password),
    		clientEncoding(SQL_DEFAULT_ENCODING),
    		allConnections(maxconns),
    		closed(false),
    		sslActive(ssl) {
	parseUrl(url);
}


EConnectionPool::EConnectionPool(int maxconns,
		const char* dbtype, const char* database,
		const char* host, const int port, const char* username,
		const char* password, boolean ssl):
		connectTimeout(-1),
		socketTimeout(-1),
		dbType(dbtype),
		database(database),
		host(host),
		port(port),
		username(username),
		password(password),
		clientEncoding(SQL_DEFAULT_ENCODING),
		allConnections(maxconns),
		closed(false),
		sslActive(ssl) {
}

void EConnectionPool::setDefaultConnectTimeout(int seconds) {
	connectTimeout = seconds;
}

int EConnectionPool::getDefaultConnectTimeout() {
	return connectTimeout;
}

void EConnectionPool::setDefaultSocketTimeout(int seconds) {
	socketTimeout = seconds;
}

int EConnectionPool::getDefaultSocketTimeout() {
	return socketTimeout;
}

void EConnectionPool::setDefaultClientEncoding(const char* charset) {
	clientEncoding = charset;
}

EString EConnectionPool::getDefaultClientEncoding() {
	return clientEncoding;
}

void EConnectionPool::reclaim(sp<EConnection> conn) {
	//加入空闲队列
	idleConnections.put(conn);
}

sp<EConnection> EConnectionPool::getConnection() {
	sp<EConnection> conn = idleConnections.poll();
	if (conn != null) {
		return new PooledConnection(this, conn);
	}

	SYNCBLOCK(&lock) {
		if (allConnections.size() < allConnections.capacity()) {
			sp<EConnection> newConn = getConnection(dbType.c_str(), username.c_str(), password.c_str());
			allConnections.add(newConn);
			return new PooledConnection(this, newConn);
		}
	}}

	return new PooledConnection(this, idleConnections.take());
}

sp<EConnection> EConnectionPool::getConnection(const char* dbtype,
		const char* username, const char* password) {
	sp<EConnection> conn = new EConnection(dbtype);
	conn->setClientEncoding(getDefaultClientEncoding().c_str());
	conn->setConnectTimeout(getDefaultConnectTimeout());
	conn->setSocketTimeout(getDefaultSocketTimeout());
	conn->setSSL(sslActive);
	conn->connect(database.c_str(), host.c_str(), port, username, password);
	return conn;
}

void EConnectionPool::close() {
	SYNCBLOCK(&lock) {
		sp<EIterator<sp<EConnection> > > iter = allConnections.iterator();
		while (iter->hasNext()) {
			sp<EConnection> conn = iter->next();
			conn->close();
		}
	}}
	closed = true;
}

boolean EConnectionPool::isClosed() {
	return closed;
}

int EConnectionPool::getMaxConnections() {
	return allConnections.capacity();
}

void EConnectionPool::parseUrl(const char* url) {
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
		port = EConnection::DEFAULT_CONNECT_PORT;
	}
}

} /* namespace edb */
} /* namespace efc */
