/*
 * ECommonStatement.hh
 *
 *  Created on: 2017-6-5
 *      Author: cxxjava@163.com
 */

#ifndef ECOMMONSTATEMENT_HH_
#define ECOMMONSTATEMENT_HH_

#include "./EStatement.hh"
#include "./EResultSet.hh"

namespace efc {
namespace edb {

/**
 * 执行单SQL语句（适用于所有SQL类型，可能有不确定个数的结果集返回，如存储过程 和 函数）
 */

class ECommonStatement: public EStatement {
public:
	virtual ~ECommonStatement();

	/**
	 *
	 */
	ECommonStatement();

	/**
	 *
	 */
	ECommonStatement(EConnection *conn);

	/**
	 * 注册输出游标
	 */
	ECommonStatement& registerOutCursor();

	/**
	 * 执行SQL语句
	 */
	virtual EStatement& execute(void) THROWS(ESQLException);

	/**
	 *  Retrieves the current result as a <code>ResultSet</code> object.
	 *  This method should be called only once per result.
	 *
	 * @param if next==true then moves to this <code>Statement</code> object's next result.
	 * @return the current result as a <code>ResultSet</code> object or
	 * <code>null</code> if the result is an update count or there are no more results
	 * @exception SQLException if a database access error occurs or
	 * this method is called on a closed <code>Statement</code>
	 * @see #execute
	 */
	EResultSet* getResultSet(boolean next=false) THROWS(ESQLException);

	/**
	 *  Retrieves the current result as an update count;
	 *  if the result is a <code>ResultSet</code> object or there are no more results, -1
	 *  is returned. This method should be called only once per result.
	 *
	 * @return the current result as an update count; -1 if the current result is a
	 * <code>ResultSet</code> object or there are no more results
	 * @exception SQLException if a database access error occurs or
	 * this method is called on a closed <code>Statement</code>
	 * @see #execute
	 */
	int getUpdateCount() THROWS(ESQLException);

	/**
	 * Get errmsg after execute sql.
	 * @return The error message string
	 */
	virtual EString getErrorMessage(void);

	/**
	 * Releases this <code>Statement</code> object's database
	 * and JDBC resources immediately instead of waiting for
	 * this to happen when it is automatically closed.
	 * It is generally good practice to release resources as soon as
	 * you are finished with them to avoid tying up database
	 * resources.
	 * <P>
	 * Calling the method <code>close</code> on a <code>Statement</code>
	 * object that is already closed has no effect.
	 * <P>
	 * <B>Note:</B>When a <code>Statement</code> object is
	 * closed, its current <code>ResultSet</code> object, if one exists, is
	 * also closed.
	 *
	 * @exception SQLException if a database access error occurs
	 */
	virtual void clear() THROWS(ESQLException);

private:
	sp<EResultSet> resultSet;
	int updateCount;

	/**
	 * Moves to this <code>Statement</code> object's next result, returns
	 * <code>true</code> if it is a <code>ResultSet</code> object, and
	 * implicitly closes any current <code>ResultSet</code>
	 * object(s) obtained with the method <code>getResultSet</code>.
	 *
	 * <P>There are no more results when the following is true:
	 * <PRE>{@code
	 *     // stmt is a Statement object
	 *     ((stmt.getMoreResults() == false) && (stmt.getUpdateCount() == -1))
	 * }</PRE>
	 *
	 * @return <code>true</code> if the next result is a <code>ResultSet</code>
	 *         object; <code>false</code> if it is an update count or there are
	 *         no more results
	 * @exception SQLException if a database access error occurs or
	 * this method is called on a closed <code>Statement</code>
	 * @see #execute
	 */
	boolean getMoreResults() THROWS(ESQLException);
};

} /* namespace edb */
} /* namespace efc */
#endif /* ECOMMONSTATEMENT_HH_ */
