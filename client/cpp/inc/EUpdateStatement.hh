/*
 * EUpdateStatement.hh
 *
 *  Created on: 2017-6-5
 *      Author: cxxjava@163.com
 */

#ifndef EUPDATESTATEMENT_HH_
#define EUPDATESTATEMENT_HH_

#include "./EStatement.hh"

namespace efc {
namespace edb {

/**
 * 执行更新类SQL语句（无结果集返回，如insert、update、delete、create、drop、alter、truncate等等）
 */

class EUpdateStatement: public EStatement {
public:
	virtual ~EUpdateStatement();

	/**
	 *
	 */
	EUpdateStatement();

	/**
	 *
	 */
	EUpdateStatement(EConnection *conn);

	/**
	 * 批执行时是否失败后继续执行
	 */
	void setFailedResume(boolean resume);

	/**
	 *
	 */
	EStatement& addBatch(const char *sql) THROWS(ESQLException);
	EStatement& addBatchFormat(const char *sqlfmt, ...) THROWS(ESQLException);

	/**
	 * This parses the query and adds it to the current batch
	 *
	 * @throws SQLException if an error occurs
	 */
	EStatement& addBatch() THROWS(ESQLException);

	/**
	 * 获取已添加SQL记录数
	 */
	int getSqlCount();

	/**
	 * 执行SQL语句
	 */
	virtual EStatement& execute(void) THROWS(ESQLException);

	/**
	 * 获取第一条执行出错的SQL语句序号，无错误则返回0，其他错误返回-1
	 */
	int getFirstFailed(void);

	/**
	 * 获取失败记录数
	 */
	int getFailures(void);

	/**
	 *  Retrieves the current result as an update count;
	 *  if the result is a <code>ResultSet</code> object or there are no more results, -1
	 *  is returned. This method should be called only once per result.
	 *
	 * @param the sql index, The first sql is number 1, the
	 * second number 2, and so on.
	 * @return the current result as an update count; -1 if the current result is a
	 * <code>ResultSet</code> object or there are no more results
	 * @exception SQLException if a database access error occurs or
	 * this method is called on a closed <code>Statement</code>
	 * @see #execute
	 */
	virtual int getUpdateCount(int index) THROWS(ESQLException);

	/**
	 * 获取更新结果错误信息
	 *
	 * @param the sql index, The first sql is number 1, the
	 * second number 2, and so on.
	 * @return 对应序号SQL语句的执行结果错误信息;
	 */
	virtual EString getUpdateMessage(int index) THROWS(ESQLException);

	/**
	 * Get errmsg after execute sql.
	 *
	 * @param the sql index, The first sql is number 1, the
	 * second number 2, and so on. index 0 is the fist failed sql error message.
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
	sp<EBson> result;
	boolean isFailedResume;
};

} /* namespace edb */
} /* namespace efc */
#endif /* EUPDATESTATEMENT_HH_ */
