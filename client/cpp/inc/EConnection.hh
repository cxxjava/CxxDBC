/*
 * EDBConnection.hh
 *
 *  Created on: 2017-5-24
 *      Author: cxxjava@163.com
 */

#ifndef EDB_CONNECTION_HH_
#define EDB_CONNECTION_HH_

#include "Efc.hh"
#include "../config.hh"
#include "./EDBHandler.hh"
#include "./ESavepoint.hh"
#include "./EDatabaseMetaData.hh"
#include "./ECommonStatement.hh"
#include "./EUpdateStatement.hh"

namespace efc {
namespace edb {

/**
 * <P>A connection (session) with a specific
 * database. SQL statements are executed and results are returned
 * within the context of a connection.
 * <P>
 * A <code>Connection</code> object's database is able to provide information
 * describing its tables, its supported SQL grammar, its stored
 * procedures, the capabilities of this connection, and so on. This
 * information is obtained with the <code>getMetaData</code> method.
 *
 * <P><B>Note:</B> When configuring a <code>Connection</code>, JDBC applications
 *  should use the appropriate <code>Connection</code> method such as
 *  <code>setAutoCommit</code> or <code>setTransactionIsolation</code>.
 *  Applications should not invoke SQL commands directly to change the connection's
 *   configuration when there is a JDBC method available.  By default a <code>Connection</code> object is in
 * auto-commit mode, which means that it automatically commits changes
 * after executing each statement. If auto-commit mode has been
 * disabled, the method <code>commit</code> must be called explicitly in
 * order to commit changes; otherwise, database changes will not be saved.
 * <P>
 * A new <code>Connection</code> object created using the JDBC 2.1 core API
 * has an initially empty type map associated with it. A user may enter a
 * custom mapping for a UDT in this type map.
 * When a UDT is retrieved from a data source with the
 * method <code>ResultSet.getObject</code>, the <code>getObject</code> method
 * will check the connection's type map to see if there is an entry for that
 * UDT.  If so, the <code>getObject</code> method will map the UDT to the
 * class indicated.  If there is no entry, the UDT will be mapped using the
 * standard mapping.
 * <p>
 * A user may create a new type map, which is a <code>java.util.Map</code>
 * object, make an entry in it, and pass it to the <code>java.sql</code>
 * methods that can perform custom mapping.  In this case, the method
 * will use the given type map instead of the one associated with
 * the connection.
 * <p>
 * For example, the following code fragment specifies that the SQL
 * type <code>ATHLETES</code> will be mapped to the class
 * <code>Athletes</code> in the Java programming language.
 * The code fragment retrieves the type map for the <code>Connection
 * </code> object <code>con</code>, inserts the entry into it, and then sets
 * the type map with the new entry as the connection's type map.
 * <pre>
 *      java.util.Map map = con.getTypeMap();
 *      map.put("mySchemaName.ATHLETES", Class.forName("Athletes"));
 *      con.setTypeMap(map);
 * </pre>
 *
 * @see DriverManager#getConnection
 * @see Statement
 * @see ResultSet
 * @see DatabaseMetaData
 */

class EConnection: virtual public EObject {
public:
	static const int DEFAULT_CONNECT_PORT = 6633;
	static const int DEFAULT_FETCH_SIZE = 8192;

	/**
	 * 设置全局默认数据库类型，该值可被私有dbtype设置覆盖。
	 * (动态加载插件模式时有效，具体值为dblib目录下动态库的文件名，如'PGSQL')
	 */
	static void setDefaultDBType(const char* dbtype);
	static const char* getDefaultDBType();

public:
	virtual ~EConnection();

	/**
	 * Creates a new instance
	 *
	 * @param dbtype 连接的数据库类型
	 */
	EConnection(boolean proxy=true);
	EConnection(const char* dbtype, boolean proxy=true);

	/**
	 *
	 */
	virtual boolean isProxyMode();

	/**
	 * @param connectTimeout connect timeout
	 */
	virtual void setConnectTimeout(int seconds);

	/**
	 * @return protocol version
	 */
	virtual int getConnectTimeout();

	/**
	 * @param seconds socket timeout
	 */
	virtual void setSocketTimeout(int seconds);

	/**
	 * @return socket timeout
	 */
	virtual int getSocketTimeout();

	/**
	 *
	 */
	virtual void setClientEncoding(const char* charset);

	/**
	 *
	 */
	virtual EString getClientEncoding();

	/**
	 *
	 */
	virtual void setSSL(boolean on);

	/**
	 *
	 */
	virtual boolean getSSL();

	/* ---------------- Connect -------------- */

	/**
	 * Connect a database with url.
	 * @param url a database url of the form
     * <code> edbc:[DBTYPE]://host:port/database?connectTimeout=xxx&socketTimeout=xxx
     *                                          &clientEncoding=xxx&username=xxx&password=xxx
     *                                          &ssl=true
     * </code>
	 */
	virtual void connect(const char *url) THROWS(ESQLException);
	virtual void connect(const char *url,
	             const char *username,
	             const char *password) THROWS(ESQLException);

	/**
	 * Connect a database with host and port.
	 */
	virtual void connect(const char *database,
                 const char *host,
                 const int   port,
                 const char *username,
                 const char *password) THROWS(ESQLException);

	/**
	 * Releases this <code>Connection</code> object's database and JDBC resources
	 * immediately instead of waiting for them to be automatically released.
	 * <P>
	 * Calling the method <code>close</code> on a <code>Connection</code>
	 * object that is already closed is a no-op.
	 * <P>
	 * It is <b>strongly recommended</b> that an application explicitly
	 * commits or rolls back an active transaction prior to calling the
	 * <code>close</code> method.  If the <code>close</code> method is called
	 * and there is an active transaction, the results are implementation-defined.
	 * <P>
	 *
	 * @exception SQLException SQLException if a database access error occurs
	 */
	virtual void close(void) THROWS(ESQLException);

	/**
	 * Retrieves whether this <code>Connection</code> object has been
	 * closed.  A connection is closed if the method <code>close</code>
	 * has been called on it or if certain fatal errors have occurred.
	 * This method is guaranteed to return <code>true</code> only when
	 * it is called after the method <code>Connection.close</code> has
	 * been called.
	 * <P>
	 * This method generally cannot be called to determine whether a
	 * connection to a database is valid or invalid.  A typical client
	 * can determine that a connection is invalid by catching any
	 * exceptions that might be thrown when an operation is attempted.
	 *
	 * @return <code>true</code> if this <code>Connection</code> object
	 *         is closed; <code>false</code> if it is still open
	 * @exception SQLException if a database access error occurs
	 */
	virtual boolean isClosed() THROWS(ESQLException);

	/**
	 * Get virtual db type.
	 * @return Virtual dbtype
	 */
	virtual EString getConnectionDatabaseType(void);

	/**
	 * Get virtual db name.
	 * @return Virtual dbname
	 */
	virtual EString getConnectionDatabaseName(void);

	/**
	 * Get the access virtual db's username.
	 * @return The username
	 */
	virtual EString getConnectionUsername(void);

	/**
	 * Get the access virtual db's password.
	 * @return The password
	 */
	virtual EString getConnectionPassword(void);

	/**
	 * Retrieves a <code>DatabaseMetaData</code> object that contains
	 * metadata about the database to which this
	 * <code>Connection</code> object represents a connection.
	 * The metadata includes information about the database's
	 * tables, its supported SQL grammar, its stored
	 * procedures, the capabilities of this connection, and so on.
	 *
	 * @return a <code>DatabaseMetaData</code> object for this
	 *         <code>Connection</code> object
	 * @exception  SQLException if a database access error occurs
	 * or this method is called on a closed connection
	 */
	virtual EDatabaseMetaData* getMetaData() THROWS(ESQLException);

	/* ---------------- Statements -------------- */

	/**
	 *
	 */
	virtual sp<ECommonStatement> createCommonStatement() THROWS(ESQLException);

	/**
	 *
	 */
	virtual sp<EUpdateStatement> createUpdateStatement() THROWS(ESQLException);

	/* ---------------- Transactions -------------- */

	/**
	 * Sets this connection's auto-commit mode to the given state.
	 * If a connection is in auto-commit mode, then all its SQL
	 * statements will be executed and committed as individual
	 * transactions.  Otherwise, its SQL statements are grouped into
	 * transactions that are terminated by a call to either
	 * the method <code>commit</code> or the method <code>rollback</code>.
	 * By default, new connections are in auto-commit
	 * mode.
	 * <P>
	 * The commit occurs when the statement completes. The time when the statement
	 * completes depends on the type of SQL Statement:
	 * <ul>
	 * <li>For DML statements, such as Insert, Update or Delete, and DDL statements,
	 * the statement is complete as soon as it has finished executing.
	 * <li>For Select statements, the statement is complete when the associated result
	 * set is closed.
	 * <li>For <code>CallableStatement</code> objects or for statements that return
	 * multiple results, the statement is complete
	 * when all of the associated result sets have been closed, and all update
	 * counts and output parameters have been retrieved.
	 *</ul>
	 * <P>
	 * <B>NOTE:</B>  If this method is called during a transaction and the
	 * auto-commit mode is changed, the transaction is committed.  If
	 * <code>setAutoCommit</code> is called and the auto-commit mode is
	 * not changed, the call is a no-op.
	 *
	 * @param autoCommit <code>true</code> to enable auto-commit mode;
	 *         <code>false</code> to disable it
	 * @exception SQLException if a database access error occurs,
	 *  setAutoCommit(true) is called while participating in a distributed transaction,
	 * or this method is called on a closed connection
	 * @see #getAutoCommit
	 */
	virtual void setAutoCommit(boolean autoCommit) THROWS(ESQLException);

	/**
	 * Retrieves the current auto-commit mode for this <code>Connection</code>
	 * object.
	 *
	 * @return the current state of this <code>Connection</code> object's
	 *         auto-commit mode
	 * @exception SQLException if a database access error occurs
	 * or this method is called on a closed connection
	 * @see #setAutoCommit
	 */
	virtual boolean getAutoCommit() THROWS(ESQLException);

	/**
	 * Makes all changes made since the previous
	 * commit/rollback permanent and releases any database locks
	 * currently held by this <code>Connection</code> object.
	 * This method should be
	 * used only when auto-commit mode has been disabled.
	 *
	 * @exception SQLException if a database access error occurs,
	 * this method is called while participating in a distributed transaction,
	 * if this method is called on a closed connection or this
	 *            <code>Connection</code> object is in auto-commit mode
	 * @see #setAutoCommit
	 */
	virtual void commit(void) THROWS(ESQLException);

	/**
	 * Undoes all changes made in the current transaction
	 * and releases any database locks currently held
	 * by this <code>Connection</code> object. This method should be
	 * used only when auto-commit mode has been disabled.
	 *
	 * @exception SQLException if a database access error occurs,
	 * this method is called while participating in a distributed transaction,
	 * this method is called on a closed connection or this
	 *            <code>Connection</code> object is in auto-commit mode
	 * @see #setAutoCommit
	 */
	virtual void rollback(void) THROWS(ESQLException);

	/**
	 * Creates an unnamed savepoint in the current transaction and
	 * returns the new <code>Savepoint</code> object that represents it.
	 *
	 *<p> if setSavepoint is invoked outside of an active transaction, a transaction will be started at this newly created
	 *savepoint.
	 *
	 * @return the new <code>Savepoint</code> object
	 * @exception SQLException if a database access error occurs,
	 * this method is called while participating in a distributed transaction,
	 * this method is called on a closed connection
	 *            or this <code>Connection</code> object is currently in
	 *            auto-commit mode
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @see Savepoint
	 * @since 1.4
	 */
	virtual ESavepoint setSavepoint() THROWS(ESQLException);

	/**
	 * Creates a savepoint with the given name in the current transaction
	 * and returns the new <code>Savepoint</code> object that represents it.
	 *
	 * <p> if setSavepoint is invoked outside of an active transaction, a transaction will be started at this newly created
	 *savepoint.
	 *
	 * @param name a <code>String</code> containing the name of the savepoint
	 * @return the new <code>Savepoint</code> object
	 * @exception SQLException if a database access error occurs,
		  * this method is called while participating in a distributed transaction,
	 * this method is called on a closed connection
	 *            or this <code>Connection</code> object is currently in
	 *            auto-commit mode
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @see Savepoint
	 * @since 1.4
	 */
	virtual ESavepoint setSavepoint(const char* name) THROWS(ESQLException);

	/**
	 * Undoes all changes made after the given <code>Savepoint</code> object
	 * was set.
	 * <P>
	 * This method should be used only when auto-commit has been disabled.
	 *
	 * @param savepoint the <code>Savepoint</code> object to roll back to
	 * @exception SQLException if a database access error occurs,
	 * this method is called while participating in a distributed transaction,
	 * this method is called on a closed connection,
	 *            the <code>Savepoint</code> object is no longer valid,
	 *            or this <code>Connection</code> object is currently in
	 *            auto-commit mode
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @see Savepoint
	 * @see #rollback
	 * @since 1.4
	 */
	virtual void rollback(ESavepoint savepoint) THROWS(ESQLException);

	/**
	 * Removes the specified <code>Savepoint</code>  and subsequent <code>Savepoint</code> objects from the current
	 * transaction. Any reference to the savepoint after it have been removed
	 * will cause an <code>SQLException</code> to be thrown.
	 *
	 * @param savepoint the <code>Savepoint</code> object to be removed
	 * @exception SQLException if a database access error occurs, this
	 *  method is called on a closed connection or
	 *            the given <code>Savepoint</code> object is not a valid
	 *            savepoint in the current transaction
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.4
	 */
	virtual void releaseSavepoint(ESavepoint savepoint) THROWS(ESQLException);

	/**
	 *
	 */
	virtual llong createLargeObject() THROWS(ESQLException);
	virtual llong writeLargeObject(llong oid, EInputStream* is) THROWS(ESQLException);
	virtual llong readLargeObject(llong oid, EOutputStream* os) THROWS(ESQLException);

protected:
	friend class ECommonStatement;
	friend class EUpdateStatement;
	friend class EResultSet;

	EDBHandler dbHandler;

	void checkClosed() THROWS(ESQLException);
	void checkConnected() THROWS(ESQLException);

private:
	boolean isProxymode;
	boolean isConnected;

	int connectTimeout;
	int socketTimeout;
	boolean sslActive;
	EString dbType;
	EString clientEncoding;
	EString database;
	EString host;
	int port;
	EString username;
	EString password;

	boolean autoCommit;

	EDatabaseMetaData metaData;

	void parseUrl(const char* url) THROWS(EIllegalArgumentException);

	void connectInternal() THROWS(ESQLException);
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDB_CONNECTION_HH_ */
