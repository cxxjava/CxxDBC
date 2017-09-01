/*
 * EResultSet.hh
 *
 *  Created on: 2017-6-6
 *      Author: cxxjava@163.com
 */

#ifndef ERESULTSET_HH_
#define ERESULTSET_HH_

#include "Efc.hh"
#include "./EStatement.hh"
#include "./EResultSetMetaData.hh"

namespace efc {
namespace edb {

/**
 *
 */

class EResultSet: public EObject {
public:

	/**
	 * Move dataset cursor to next.
	 * @return true or false
	 */
	boolean next();

	/**
	 * Releases this <code>ResultSet</code> object's database and
	 * JDBC resources immediately instead of waiting for
	 * this to happen when it is automatically closed.
	 *
	 * <P>The closing of a <code>ResultSet</code> object does <strong>not</strong> close the <code>Blob</code>,
	 * <code>Clob</code> or <code>NClob</code> objects created by the <code>ResultSet</code>. <code>Blob</code>,
	 * <code>Clob</code> or <code>NClob</code> objects remain valid for at least the duration of the
	 * transaction in which they are created, unless their <code>free</code> method is invoked.
	 *<p>
	 * When a <code>ResultSet</code> is closed, any <code>ResultSetMetaData</code>
	 * instances that were created by calling the  <code>getMetaData</code>
	 * method remain accessible.
	 *
	 * <P><B>Note:</B> A <code>ResultSet</code> object
	 * is automatically closed by the
	 * <code>Statement</code> object that generated it when
	 * that <code>Statement</code> object is closed,
	 * re-executed, or is used to retrieve the next result from a
	 * sequence of multiple results.
	 *<p>
	 * Calling the method <code>close</code> on a <code>ResultSet</code>
	 * object that is already closed is a no-op.
	 *
	 *
	 * @exception SQLException if a database access error occurs
	 */
	void close() THROWS(ESQLException);


	/**
	 * Retrieves the  number, types and properties of
	 * this <code>ResultSet</code> object's columns.
	 *
	 * @return the description of this <code>ResultSet</code> object's columns
	 * @exception SQLException if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EResultSetMetaData* getMetaData();

	/**
	 * Maps the given <code>ResultSet</code> column label to its
	 * <code>ResultSet</code> column index.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column index of the given column name
	 * @exception SQLException if the <code>ResultSet</code> object
	 * does not contain a column labeled <code>columnLabel</code>, a database access error occurs
	 *  or this method is called on a closed result set
	 */
	int findColumn(const char *columnLabel);

	/**
	 * Retrieves the current row number.  The first row is number 1, the
	 * second number 2, and so on.
	 * <p>
	 * <strong>Note:</strong>Support for the <code>getRow</code> method
	 * is optional for <code>ResultSet</code>s with a result
	 * set type of <code>TYPE_FORWARD_ONLY</code>
	 *
	 * @return the current row number; <code>0</code> if there is no current row
	 * @exception SQLException if a database access error occurs
	 * or this method is called on a closed result set
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.2
	 */
	int getRow();

	// Methods for accessing results by column index

	/**
	 * Reports whether
	 * the value of the designated column in the current row
	 * of this <code>ResultSet</code> is SQL <code>NULL</code>.
	 *
	 * @return <code>true</code> if the last column value read was SQL
	 *         <code>NULL</code> and <code>false</code> otherwise
	 * @exception SQLException if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	boolean isNull(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>String</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EString getString(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>boolean</code> in the Java programming language.
	 *
	 * <P>If the designated column has a datatype of CHAR or VARCHAR
	 * and contains a "0" or has a datatype of BIT, TINYINT, SMALLINT, INTEGER or BIGINT
	 * and contains  a 0, a value of <code>false</code> is returned.  If the designated column has a datatype
	 * of CHAR or VARCHAR
	 * and contains a "1" or has a datatype of BIT, TINYINT, SMALLINT, INTEGER or BIGINT
	 * and contains  a 1, a value of <code>true</code> is returned.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>false</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	boolean getBoolean(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>byte</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	byte getByte(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>short</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	short getShort(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * an <code>int</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	int getInt(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>long</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	llong getLLong(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>float</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	float getFloat(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>double</code> in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	double getDouble(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>byte</code> array in the Java programming language.
	 * The bytes represent the raw values returned by the driver.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EA<byte> getBytes(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>java.sql.Date</code> object in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EDate getDate(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a
	 * <code>java.math.BigDecimal</code> with full precision.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return the column value (full precision);
	 * if the value is SQL <code>NULL</code>, the value returned is
	 * <code>null</code> in the Java programming language.
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 * @since 1.2
	 */
	EBigDecimal getBigDecimal(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a stream of ASCII characters. The value can then be read in chunks from the
	 * stream. This method is particularly
	 * suitable for retrieving large <code>LONGVARCHAR</code> values.
	 * The JDBC driver will
	 * do any necessary conversion from the database format into ASCII.
	 *
	 * <P><B>Note:</B> All the data in the returned stream must be
	 * read prior to getting the value of any other column. The next
	 * call to a getter method implicitly closes the stream.  Also, a
	 * stream may return <code>0</code> when the method
	 * <code>InputStream.available</code>
	 * is called whether there is data available or not.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return a Java input stream that delivers the database column value
	 * as a stream of one-byte ASCII characters;
	 * if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	sp<EInputStream> getAsciiStream(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a  stream of
	 * uninterpreted bytes. The value can then be read in chunks from the
	 * stream. This method is particularly
	 * suitable for retrieving large <code>LONGVARBINARY</code> values.
	 *
	 * <P><B>Note:</B> All the data in the returned stream must be
	 * read prior to getting the value of any other column. The next
	 * call to a getter method implicitly closes the stream.  Also, a
	 * stream may return <code>0</code> when the method
	 * <code>InputStream.available</code>
	 * is called whether there is data available or not.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return a Java input stream that delivers the database column value
	 *         as a stream of uninterpreted bytes;
	 *         if the value is SQL <code>NULL</code>, the value returned is
	 *         <code>null</code>
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	sp<EInputStream> getBinaryStream(int columnIndex) THROWS(ESQLException);


	// Methods for accessing results by column label

	/**
	 * Reports whether
	 * the value of the designated column in the current row
	 * of this <code>ResultSet</code> is SQL <code>NULL</code>.
	 *
	 * @return <code>true</code> if the last column value read was SQL
	 *         <code>NULL</code> and <code>false</code> otherwise
	 * @exception SQLException if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	boolean isNull(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>String</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EString getString(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>boolean</code> in the Java programming language.
	 *
	 * <P>If the designated column has a datatype of CHAR or VARCHAR
	 * and contains a "0" or has a datatype of BIT, TINYINT, SMALLINT, INTEGER or BIGINT
	 * and contains  a 0, a value of <code>false</code> is returned.  If the designated column has a datatype
	 * of CHAR or VARCHAR
	 * and contains a "1" or has a datatype of BIT, TINYINT, SMALLINT, INTEGER or BIGINT
	 * and contains  a 1, a value of <code>true</code> is returned.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>false</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	boolean getBoolean(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>byte</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	byte getByte(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>short</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	short getShort(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * an <code>int</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	int getInt(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>long</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	llong getLLong(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>float</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	float getFloat(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>double</code> in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>0</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	double getDouble(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>byte</code> array in the Java programming language.
	 * The bytes represent the raw values returned by the driver.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EA<byte> getBytes(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as
	 * a <code>java.sql.Date</code> object in the Java programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value; if the value is SQL <code>NULL</code>, the
	 * value returned is <code>null</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	EDate getDate(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a
	 * <code>java.math.BigDecimal</code> with full precision.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value (full precision);
	 * if the value is SQL <code>NULL</code>, the value returned is
	 * <code>null</code> in the Java programming language.
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 * @since 1.2
	 *
	 */
	EBigDecimal getBigDecimal(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a stream of
	 * ASCII characters. The value can then be read in chunks from the
	 * stream. This method is particularly
	 * suitable for retrieving large <code>LONGVARCHAR</code> values.
	 * The JDBC driver will
	 * do any necessary conversion from the database format into ASCII.
	 *
	 * <P><B>Note:</B> All the data in the returned stream must be
	 * read prior to getting the value of any other column. The next
	 * call to a getter method implicitly closes the stream. Also, a
	 * stream may return <code>0</code> when the method <code>available</code>
	 * is called whether there is data available or not.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return a Java input stream that delivers the database column value
	 * as a stream of one-byte ASCII characters.
	 * If the value is SQL <code>NULL</code>,
	 * the value returned is <code>null</code>.
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	sp<EInputStream> getAsciiStream(const char* columnLabel) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a stream of uninterpreted
	 * <code>byte</code>s.
	 * The value can then be read in chunks from the
	 * stream. This method is particularly
	 * suitable for retrieving large <code>LONGVARBINARY</code>
	 * values.
	 *
	 * <P><B>Note:</B> All the data in the returned stream must be
	 * read prior to getting the value of any other column. The next
	 * call to a getter method implicitly closes the stream. Also, a
	 * stream may return <code>0</code> when the method <code>available</code>
	 * is called whether there is data available or not.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return a Java input stream that delivers the database column value
	 * as a stream of uninterpreted bytes;
	 * if the value is SQL <code>NULL</code>, the result is <code>null</code>
	 * @exception SQLException if the columnLabel is not valid;
	 * if a database access error occurs or this method is
	 *            called on a closed result set
	 */
	sp<EInputStream> getBinaryStream(const char* columnLabel) THROWS(ESQLException);

#if 0
	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a <code>Blob</code> object
	 * in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return a <code>Blob</code> object representing the SQL
	 *         <code>BLOB</code> value in the specified column
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs
	 * or this method is called on a closed result set
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.2
	 */
	Blob getBlob(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row
	 * of this <code>ResultSet</code> object as a <code>Clob</code> object
	 * in the Java programming language.
	 *
	 * @param columnIndex the first column is 1, the second is 2, ...
	 * @return a <code>Clob</code> object representing the SQL
	 *         <code>CLOB</code> value in the specified column
	 * @exception SQLException if the columnIndex is not valid;
	 * if a database access error occurs
	 * or this method is called on a closed result set
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.2
	 */
	Clob getClob(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row of this
	 * <code>ResultSet</code> object as a <code>java.sql.RowId</code> object in the Java
	 * programming language.
	 *
	 * @param columnIndex the first column is 1, the second 2, ...
	 * @return the column value; if the value is a SQL <code>NULL</code> the
	 *     value returned is <code>null</code>
	 * @THROWS(ESQLException) if the columnIndex is not valid;
	 * if a database access error occurs
	 * or this method is called on a closed result set
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.6
	 */
	RowId getRowId(int columnIndex) THROWS(ESQLException);

	/**
	 * Retrieves the value of the designated column in the current row of this
	 * <code>ResultSet</code> object as a <code>java.sql.RowId</code> object in the Java
	 * programming language.
	 *
	 * @param columnLabel the label for the column specified with the SQL AS clause.  If the SQL AS clause was not specified, then the label is the name of the column
	 * @return the column value ; if the value is a SQL <code>NULL</code> the
	 *     value returned is <code>null</code>
	 * @THROWS(ESQLException) if the columnLabel is not valid;
	 * if a database access error occurs
	 * or this method is called on a closed result set
	 * @exception SQLFeatureNotSupportedException if the JDBC driver does not support
	 * this method
	 * @since 1.6
	 */
	RowId getRowId(String columnLabel) THROWS(ESQLException);
#endif

private:
	friend class ECommonStatement;

	EStatement* statement;
	sp<EBson> dataset;
	EResultSetMetaData metaData;
	llong cursorID;
	es_bson_node_t* currRow;
	llong rows;

	EResultSet(EStatement* stmt, sp<EBson> rep);

	void fetch() THROWS(ESQLException);

	void* getColumnData(int columnIndex, es_size_t *size) THROWS(EIndexOutOfBoundsException);
};

} /* namespace edb */
} /* namespace efc */
#endif /* ERESULTSET_HH_ */
