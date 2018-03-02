/*
 * EResultSetMetaData.hh
 *
 *  Created on: 2017-6-7
 *      Author: cxxjava@163.com
 */

#ifndef ERESULTSETMETADATA_HH_
#define ERESULTSETMETADATA_HH_

#include "Efc.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

/**
 * An object that can be used to get information about the types
 * and properties of the columns in a <code>ResultSet</code> object.
 * The following code fragment creates the <code>ResultSet</code> object rs,
 * creates the <code>ResultSetMetaData</code> object rsmd, and uses rsmd
 * to find out how many columns rs has and whether the first column in rs
 * can be used in a <code>WHERE</code> clause.
 * <PRE>
 *
 *     ResultSet rs = stmt.executeQuery("SELECT a, b, c FROM TABLE2");
 *     ResultSetMetaData rsmd = rs.getMetaData();
 *     int numberOfColumns = rsmd.getColumnCount();
 *
 * </PRE>
 */

class EResultSetMetaData: public EObject {
public:
	/**
	 * Returns the number of columns in this <code>ResultSet</code> object.
	 *
	 * @return the number of columns
	 * @exception SQLException if a database access error occurs
	 */
	int	getColumnCount();

	/**
	 * Gets the designated column's suggested title for use in printouts and
	 * displays. The suggested title is usually specified by the SQL <code>AS</code>
	 * clause.  If a SQL <code>AS</code> is not specified, the value returned from
	 * <code>getColumnLabel</code> will be the same as the value returned by the
	 * <code>getColumnName</code> method.
	 *
	 * @param column the first column is 1, the second is 2, ...
	 * @return the suggested column title
	 * @exception SQLException if a database access error occurs
	 */
	EString getColumnLabel(int column);

	/**
	 * Get the designated column's specified column size.
	 * For numeric data, this is the maximum precision.  For character data, this is the length in characters.
	 * For datetime datatypes, this is the length in characters of the String representation (assuming the
	 * maximum allowed precision of the fractional seconds component). For binary data, this is the length in bytes.  For the ROWID datatype,
	 * this is the length in bytes. 0 is returned for data types where the
	 * column size is not applicable.
	 *
	 * @param column the first column is 1, the second is 2, ...
	 * @return precision
	 * @exception SQLException if a database access error occurs
	 */
	int getPrecision(int column);

	/**
	 * Gets the designated column's number of digits to right of the decimal point.
	 * 0 is returned for data types where the scale is not applicable.
	 *
	 * @param column the first column is 1, the second is 2, ...
	 * @return scale
	 * @exception SQLException if a database access error occurs
	 */
	int getScale(int column);

	/**
	 * Retrieves the designated column's SQL type.
	 *
	 * @param column the first column is 1, the second is 2, ...
	 * @return SQL type from java.sql.Types
	 * @exception SQLException if a database access error occurs
	 * @see Types
	 */
	edb_field_type_e getColumnType(int column);

	/**
	 * Indicates the designated column's normal maximum width in characters.
	 *
	 * @param column the first column is 1, the second is 2, ...
	 * @return the normal maximum number of characters allowed as the width
	 *          of the designated column
	 * @exception SQLException if a database access error occurs
	 */
	int getColumnDisplaySize(int column);

	/**
	 *
	 */
	virtual EString toString();

private:
	friend class EResultSet;

	sp<EBson> fields;
	int columns;

	EResultSetMetaData(sp<EBson> rep);
};

} /* namespace edb */
} /* namespace efc */
#endif /* ERESULTSETMETADATA_HH_ */
