/*
 * EConnectionPool.hh
 *
 *  Created on: 2017-7-4
 *      Author: cxxjava@163.com
 */

#ifndef ECONNECTIONPOOL_HH_
#define ECONNECTIONPOOL_HH_

#include "Efc.hh"
#include "./EConnection.hh"

namespace efc {
namespace edb {

/**
 * An object that provides hooks for connection pool management.
 * A <code>PooledConnection</code> object
 * represents a physical connection to a data source.  The connection
 * can be recycled rather than being closed when an application is
 * finished with it, thus reducing the number of connections that
 * need to be made.
 * <P>
 * An application programmer does not use the <code>PooledConnection</code>
 * interface directly; rather, it is used by a middle tier infrastructure
 * that manages the pooling of connections.
 * <P>
 * When an application calls the method <code>DataSource.getConnection</code>,
 * it gets back a <code>Connection</code> object.  If connection pooling is
 * being done, that <code>Connection</code> object is actually a handle to
 * a <code>PooledConnection</code> object, which is a physical connection.
 * <P>
 * The connection pool manager, typically the application server, maintains
 * a pool of <code>PooledConnection</code> objects.  If there is a
 * <code>PooledConnection</code> object available in the pool, the
 * connection pool manager returns a <code>Connection</code> object that
 * is a handle to that physical connection.
 * If no <code>PooledConnection</code> object is available, the
 * connection pool manager calls the <code>ConnectionPoolDataSource</code>
 * method <code>getPoolConnection</code> to create a new physical connection.  The
 *  JDBC driver implementing <code>ConnectionPoolDataSource</code> creates a
 *  new <code>PooledConnection</code> object and returns a handle to it.
 * <P>
 * When an application closes a connection, it calls the <code>Connection</code>
 * method <code>close</code>. When connection pooling is being done,
 * the connection pool manager is notified because it has registered itself as
 * a <code>ConnectionEventListener</code> object using the
 * <code>ConnectionPool</code> method <code>addConnectionEventListener</code>.
 * The connection pool manager deactivates the handle to
 * the <code>PooledConnection</code> object and  returns the
 * <code>PooledConnection</code> object to the pool of connections so that
 * it can be used again.  Thus, when an application closes its connection,
 * the underlying physical connection is recycled rather than being closed.
 * <P>
 * The physical connection is not closed until the connection pool manager
 * calls the <code>PooledConnection</code> method <code>close</code>.
 * This method is generally called to have an orderly shutdown of the server or
 * if a fatal error has made the connection unusable.
 *
 * <p>
 * A connection pool manager is often also a statement pool manager, maintaining
 *  a pool of <code>PreparedStatement</code> objects.
 *  When an application closes a prepared statement, it calls the
 *  <code>PreparedStatement</code>
 * method <code>close</code>. When <code>Statement</code> pooling is being done,
 * the pool manager is notified because it has registered itself as
 * a <code>StatementEventListener</code> object using the
 * <code>ConnectionPool</code> method <code>addStatementEventListener</code>.
 *  Thus, when an application closes its  <code>PreparedStatement</code>,
 * the underlying prepared statement is recycled rather than being closed.
 * <P>
 *
 * @since 1.4
 */

class EConnectionPool: public EObject {
public:

	/**
	 * @param url a database url of the form
     * <code> edbc:[DBTYPE]://host:port/database?connectTimeout=xxx&socketTimeout=xxx&clientEncoding=xxx</code>
	 */
	EConnectionPool(int maxconns,
			const char *url,
            const char *username,
            const char *password,
            boolean ssl=false);

	EConnectionPool(int maxconns,
			const char* dbtype,
			const char *database,
            const char *host,
            const int   port,
            const char *username,
            const char *password,
            boolean ssl=false);

	/**
	 * @param connectTimeout connect timeout
	 */
	virtual void setDefaultConnectTimeout(int seconds);

	/**
	 * @return protocol version
	 */
	virtual int getDefaultConnectTimeout();

	/**
	 * @param seconds socket timeout
	 */
	virtual void setDefaultSocketTimeout(int seconds);

	/**
	 * @return socket timeout
	 */
	virtual int getDefaultSocketTimeout();

	/**
	 *
	 */
	virtual void setDefaultClientEncoding(const char* charset);

	/**
	 *
	 */
	virtual EString getDefaultClientEncoding();

	/**
	 * Gets a connection from the connection pool.
	 *
	 * @return A pooled connection.
	 * @throws SQLException Occurs when no pooled connection is available, and a new physical
	 *         connection cannot be created.
	 */
	virtual sp<EConnection> getConnection() THROWS(ESQLException);

	/**
	 * Gets a <b>non-pooled</b> connection, unless the user and password are the same as the default
	 * values for this connection pool.
	 *
	 * @return A pooled connection.
	 * @throws SQLException Occurs when no pooled connection is available, and a new physical
	 *         connection cannot be created.
	 */
	virtual sp<EConnection> getConnection(const char* dbtype,
			const char* username, const char* password) THROWS(ESQLException);

	/**
	 * Closes the physical connection that this <code>PooledConnection</code>
	 * object represents.  An application never calls this method directly;
	 * it is called by the connection pool module, or manager.
	 * <P>
	 * See the {@link PooledConnection interface description} for more
	 * information.
	 *
	 * @exception SQLException if a database access error occurs
	 * @exception java.sql.SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.4
	 */
	virtual void close() THROWS(ESQLException);

	/**
	 *
	 */
	virtual boolean isClosed() THROWS(ESQLException);

	/**
	 * Gets the maximum number of connections that the pool will allow. If a request comes in and this
	 * many connections are in use, the request will block until a connection is available. Note that
	 * connections for a user other than the default user will not be pooled and don't count against
	 * this limit.
	 *
	 * @return The maximum number of pooled connection allowed, or 0 for no maximum.
	 */
	virtual int getMaxConnections();

private:
	friend class PooledConnection;

	int connectTimeout;
	int socketTimeout;
	EString dbType;
	EString clientEncoding;
	EString database;
	EString host;
	int port;
	EString username;
	EString password;
	boolean sslActive;

	EReentrantLock lock;
	EArrayList<sp<EConnection> > allConnections;
	ELinkedBlockingQueue<EConnection> idleConnections;
	boolean closed;

	void parseUrl(const char* url) THROWS(EIllegalArgumentException);
	void reclaim(sp<EConnection> conn);
};

} /* namespace edb */
} /* namespace efc */
#endif /* ECONNECTIONPOOL_HH_ */
