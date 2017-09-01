/*
 * EDatabaseMetaData.hh
 *
 *  Created on: 2017-6-7
 *      Author: cxxjava@163.com
 */

#ifndef EDATABASEMETADATA_HH_
#define EDATABASEMETADATA_HH_

#include "Efc.hh"

namespace efc {
namespace edb {

/**
 * Comprehensive information about the database as a whole.
 * <P>
 * This interface is implemented by driver vendors to let users know the capabilities
 * of a Database Management System (DBMS) in combination with
 * the driver based on JDBC&trade; technology
 * ("JDBC driver") that is used with it.  Different relational DBMSs often support
 * different features, implement features in different ways, and use different
 * data types.  In addition, a driver may implement a feature on top of what the
 * DBMS offers.  Information returned by methods in this interface applies
 * to the capabilities of a particular driver and a particular DBMS working
 * together. Note that as used in this documentation, the term "database" is
 * used generically to refer to both the driver and DBMS.
 * <P>
 * A user for this interface is commonly a tool that needs to discover how to
 * deal with the underlying DBMS.  This is especially true for applications
 * that are intended to be used with more than one DBMS. For example, a tool might use the method
 * <code>getTypeInfo</code> to find out what data types can be used in a
 * <code>CREATE TABLE</code> statement.  Or a user might call the method
 * <code>supportsCorrelatedSubqueries</code> to see if it is possible to use
 * a correlated subquery or <code>supportsBatchUpdates</code> to see if it is
 * possible to use batch updates.
 * <P>
 * Some <code>DatabaseMetaData</code> methods return lists of information
 * in the form of <code>ResultSet</code> objects.
 * Regular <code>ResultSet</code> methods, such as
 * <code>getString</code> and <code>getInt</code>, can be used
 * to retrieve the data from these <code>ResultSet</code> objects.  If
 * a given form of metadata is not available, an empty <code>ResultSet</code>
 * will be returned. Additional columns beyond the columns defined to be
 * returned by the <code>ResultSet</code> object for a given method
 * can be defined by the JDBC driver vendor and must be accessed
 * by their <B>column label</B>.
 * <P>
 * Some <code>DatabaseMetaData</code> methods take arguments that are
 * String patterns.  These arguments all have names such as fooPattern.
 * Within a pattern String, "%" means match any substring of 0 or more
 * characters, and "_" means match any one character. Only metadata
 * entries matching the search pattern are returned. If a search pattern
 * argument is set to <code>null</code>, that argument's criterion will
 * be dropped from the search.
 *
 */

class EDatabaseMetaData: public EObject {
public:
	/**
	 * Retrieves the name of this database product.
	 *
	 * @return database product name
	 * @exception SQLException if a database access error occurs
	 */
	EString getDatabaseProductName();

	/**
	 * Retrieves the version number of this database product.
	 *
	 * @return database version number
	 * @exception SQLException if a database access error occurs
	 */
	EString getDatabaseProductVersion();

#if 0
	/**
	 * Retrieves a description of the tables available in the given catalog.
	 * Only table descriptions matching the catalog, schema, table
	 * name and type criteria are returned.  They are ordered by
	 * <code>TABLE_TYPE</code>, <code>TABLE_CAT</code>,
	 * <code>TABLE_SCHEM</code> and <code>TABLE_NAME</code>.
	 * <P>
	 * Each table description has the following columns:
	 *  <OL>
	 *  <LI><B>TABLE_CAT</B> String {@code =>} table catalog (may be <code>null</code>)
	 *  <LI><B>TABLE_SCHEM</B> String {@code =>} table schema (may be <code>null</code>)
	 *  <LI><B>TABLE_NAME</B> String {@code =>} table name
	 *  <LI><B>TABLE_TYPE</B> String {@code =>} table type.  Typical types are "TABLE",
	 *                  "VIEW", "SYSTEM TABLE", "GLOBAL TEMPORARY",
	 *                  "LOCAL TEMPORARY", "ALIAS", "SYNONYM".
	 *  <LI><B>REMARKS</B> String {@code =>} explanatory comment on the table
	 *  <LI><B>TYPE_CAT</B> String {@code =>} the types catalog (may be <code>null</code>)
	 *  <LI><B>TYPE_SCHEM</B> String {@code =>} the types schema (may be <code>null</code>)
	 *  <LI><B>TYPE_NAME</B> String {@code =>} type name (may be <code>null</code>)
	 *  <LI><B>SELF_REFERENCING_COL_NAME</B> String {@code =>} name of the designated
	 *                  "identifier" column of a typed table (may be <code>null</code>)
	 *  <LI><B>REF_GENERATION</B> String {@code =>} specifies how values in
	 *                  SELF_REFERENCING_COL_NAME are created. Values are
	 *                  "SYSTEM", "USER", "DERIVED". (may be <code>null</code>)
	 *  </OL>
	 *
	 * <P><B>Note:</B> Some databases may not return information for
	 * all tables.
	 *
	 * @param catalog a catalog name; must match the catalog name as it
	 *        is stored in the database; "" retrieves those without a catalog;
	 *        <code>null</code> means that the catalog name should not be used to narrow
	 *        the search
	 * @param schemaPattern a schema name pattern; must match the schema name
	 *        as it is stored in the database; "" retrieves those without a schema;
	 *        <code>null</code> means that the schema name should not be used to narrow
	 *        the search
	 * @param tableNamePattern a table name pattern; must match the
	 *        table name as it is stored in the database
	 * @param types a list of table types, which must be from the list of table types
	 *         returned from {@link #getTableTypes},to include; <code>null</code> returns
	 * all types
	 * @return <code>ResultSet</code> - each row is a table description
	 * @exception SQLException if a database access error occurs
	 * @see #getSearchStringEscape
	 */
	sp<EResultSet> getTables(String catalog, String schemaPattern,
						String tableNamePattern, String types[]) throws SQLException;

	/**
	 * Retrieves the schema names available in this database.  The results
	 * are ordered by <code>TABLE_CATALOG</code> and
	 * <code>TABLE_SCHEM</code>.
	 *
	 * <P>The schema columns are:
	 *  <OL>
	 *  <LI><B>TABLE_SCHEM</B> String {@code =>} schema name
	 *  <LI><B>TABLE_CATALOG</B> String {@code =>} catalog name (may be <code>null</code>)
	 *  </OL>
	 *
	 * @return a <code>ResultSet</code> object in which each row is a
	 *         schema description
	 * @exception SQLException if a database access error occurs
	 *
	 */
	ResultSet getSchemas() throws SQLException;

	/**
	 * Retrieves the catalog names available in this database.  The results
	 * are ordered by catalog name.
	 *
	 * <P>The catalog column is:
	 *  <OL>
	 *  <LI><B>TABLE_CAT</B> String {@code =>} catalog name
	 *  </OL>
	 *
	 * @return a <code>ResultSet</code> object in which each row has a
	 *         single <code>String</code> column that is a catalog name
	 * @exception SQLException if a database access error occurs
	 */
	ResultSet getCatalogs() throws SQLException;

	/**
	 * Retrieves a description of table columns available in
	 * the specified catalog.
	 *
	 * <P>Only column descriptions matching the catalog, schema, table
	 * and column name criteria are returned.  They are ordered by
	 * <code>TABLE_CAT</code>,<code>TABLE_SCHEM</code>,
	 * <code>TABLE_NAME</code>, and <code>ORDINAL_POSITION</code>.
	 *
	 * <P>Each column description has the following columns:
	 *  <OL>
	 *  <LI><B>TABLE_CAT</B> String {@code =>} table catalog (may be <code>null</code>)
	 *  <LI><B>TABLE_SCHEM</B> String {@code =>} table schema (may be <code>null</code>)
	 *  <LI><B>TABLE_NAME</B> String {@code =>} table name
	 *  <LI><B>COLUMN_NAME</B> String {@code =>} column name
	 *  <LI><B>DATA_TYPE</B> int {@code =>} SQL type from java.sql.Types
	 *  <LI><B>TYPE_NAME</B> String {@code =>} Data source dependent type name,
	 *  for a UDT the type name is fully qualified
	 *  <LI><B>COLUMN_SIZE</B> int {@code =>} column size.
	 *  <LI><B>BUFFER_LENGTH</B> is not used.
	 *  <LI><B>DECIMAL_DIGITS</B> int {@code =>} the number of fractional digits. Null is returned for data types where
	 * DECIMAL_DIGITS is not applicable.
	 *  <LI><B>NUM_PREC_RADIX</B> int {@code =>} Radix (typically either 10 or 2)
	 *  <LI><B>NULLABLE</B> int {@code =>} is NULL allowed.
	 *      <UL>
	 *      <LI> columnNoNulls - might not allow <code>NULL</code> values
	 *      <LI> columnNullable - definitely allows <code>NULL</code> values
	 *      <LI> columnNullableUnknown - nullability unknown
	 *      </UL>
	 *  <LI><B>REMARKS</B> String {@code =>} comment describing column (may be <code>null</code>)
	 *  <LI><B>COLUMN_DEF</B> String {@code =>} default value for the column, which should be interpreted as a string when the value is enclosed in single quotes (may be <code>null</code>)
	 *  <LI><B>SQL_DATA_TYPE</B> int {@code =>} unused
	 *  <LI><B>SQL_DATETIME_SUB</B> int {@code =>} unused
	 *  <LI><B>CHAR_OCTET_LENGTH</B> int {@code =>} for char types the
	 *       maximum number of bytes in the column
	 *  <LI><B>ORDINAL_POSITION</B> int {@code =>} index of column in table
	 *      (starting at 1)
	 *  <LI><B>IS_NULLABLE</B> String  {@code =>} ISO rules are used to determine the nullability for a column.
	 *       <UL>
	 *       <LI> YES           --- if the column can include NULLs
	 *       <LI> NO            --- if the column cannot include NULLs
	 *       <LI> empty string  --- if the nullability for the
	 * column is unknown
	 *       </UL>
	 *  <LI><B>SCOPE_CATALOG</B> String {@code =>} catalog of table that is the scope
	 *      of a reference attribute (<code>null</code> if DATA_TYPE isn't REF)
	 *  <LI><B>SCOPE_SCHEMA</B> String {@code =>} schema of table that is the scope
	 *      of a reference attribute (<code>null</code> if the DATA_TYPE isn't REF)
	 *  <LI><B>SCOPE_TABLE</B> String {@code =>} table name that this the scope
	 *      of a reference attribute (<code>null</code> if the DATA_TYPE isn't REF)
	 *  <LI><B>SOURCE_DATA_TYPE</B> short {@code =>} source type of a distinct type or user-generated
	 *      Ref type, SQL type from java.sql.Types (<code>null</code> if DATA_TYPE
	 *      isn't DISTINCT or user-generated REF)
	 *   <LI><B>IS_AUTOINCREMENT</B> String  {@code =>} Indicates whether this column is auto incremented
	 *       <UL>
	 *       <LI> YES           --- if the column is auto incremented
	 *       <LI> NO            --- if the column is not auto incremented
	 *       <LI> empty string  --- if it cannot be determined whether the column is auto incremented
	 *       </UL>
	 *   <LI><B>IS_GENERATEDCOLUMN</B> String  {@code =>} Indicates whether this is a generated column
	 *       <UL>
	 *       <LI> YES           --- if this a generated column
	 *       <LI> NO            --- if this not a generated column
	 *       <LI> empty string  --- if it cannot be determined whether this is a generated column
	 *       </UL>
	 *  </OL>
	 *
	 * <p>The COLUMN_SIZE column specifies the column size for the given column.
	 * For numeric data, this is the maximum precision.  For character data, this is the length in characters.
	 * For datetime datatypes, this is the length in characters of the String representation (assuming the
	 * maximum allowed precision of the fractional seconds component). For binary data, this is the length in bytes.  For the ROWID datatype,
	 * this is the length in bytes. Null is returned for data types where the
	 * column size is not applicable.
	 *
	 * @param catalog a catalog name; must match the catalog name as it
	 *        is stored in the database; "" retrieves those without a catalog;
	 *        <code>null</code> means that the catalog name should not be used to narrow
	 *        the search
	 * @param schemaPattern a schema name pattern; must match the schema name
	 *        as it is stored in the database; "" retrieves those without a schema;
	 *        <code>null</code> means that the schema name should not be used to narrow
	 *        the search
	 * @param tableNamePattern a table name pattern; must match the
	 *        table name as it is stored in the database
	 * @param columnNamePattern a column name pattern; must match the column
	 *        name as it is stored in the database
	 * @return <code>ResultSet</code> - each row is a column description
	 * @exception SQLException if a database access error occurs
	 * @see #getSearchStringEscape
	 */
	ResultSet getColumns(String catalog, String schemaPattern,
						 String tableNamePattern, String columnNamePattern)
		throws SQLException;

	/**
	 * Retrieves a description of the stored procedures available in the given
	 * catalog.
	 * <P>
	 * Only procedure descriptions matching the schema and
	 * procedure name criteria are returned.  They are ordered by
	 * <code>PROCEDURE_CAT</code>, <code>PROCEDURE_SCHEM</code>,
	 * <code>PROCEDURE_NAME</code> and <code>SPECIFIC_ NAME</code>.
	 *
	 * <P>Each procedure description has the the following columns:
	 *  <OL>
	 *  <LI><B>PROCEDURE_CAT</B> String {@code =>} procedure catalog (may be <code>null</code>)
	 *  <LI><B>PROCEDURE_SCHEM</B> String {@code =>} procedure schema (may be <code>null</code>)
	 *  <LI><B>PROCEDURE_NAME</B> String {@code =>} procedure name
	 *  <LI> reserved for future use
	 *  <LI> reserved for future use
	 *  <LI> reserved for future use
	 *  <LI><B>REMARKS</B> String {@code =>} explanatory comment on the procedure
	 *  <LI><B>PROCEDURE_TYPE</B> short {@code =>} kind of procedure:
	 *      <UL>
	 *      <LI> procedureResultUnknown - Cannot determine if  a return value
	 *       will be returned
	 *      <LI> procedureNoResult - Does not return a return value
	 *      <LI> procedureReturnsResult - Returns a return value
	 *      </UL>
	 *  <LI><B>SPECIFIC_NAME</B> String  {@code =>} The name which uniquely identifies this
	 * procedure within its schema.
	 *  </OL>
	 * <p>
	 * A user may not have permissions to execute any of the procedures that are
	 * returned by <code>getProcedures</code>
	 *
	 * @param catalog a catalog name; must match the catalog name as it
	 *        is stored in the database; "" retrieves those without a catalog;
	 *        <code>null</code> means that the catalog name should not be used to narrow
	 *        the search
	 * @param schemaPattern a schema name pattern; must match the schema name
	 *        as it is stored in the database; "" retrieves those without a schema;
	 *        <code>null</code> means that the schema name should not be used to narrow
	 *        the search
	 * @param procedureNamePattern a procedure name pattern; must match the
	 *        procedure name as it is stored in the database
	 * @return <code>ResultSet</code> - each row is a procedure description
	 * @exception SQLException if a database access error occurs
	 * @see #getSearchStringEscape
	 */
	ResultSet getProcedures(String catalog, String schemaPattern,
							String procedureNamePattern) throws SQLException;

	/**
	 * Retrieves a description of the given table's primary key columns.  They
	 * are ordered by COLUMN_NAME.
	 *
	 * <P>Each primary key column description has the following columns:
	 *  <OL>
	 *  <LI><B>TABLE_CAT</B> String {@code =>} table catalog (may be <code>null</code>)
	 *  <LI><B>TABLE_SCHEM</B> String {@code =>} table schema (may be <code>null</code>)
	 *  <LI><B>TABLE_NAME</B> String {@code =>} table name
	 *  <LI><B>COLUMN_NAME</B> String {@code =>} column name
	 *  <LI><B>KEY_SEQ</B> short {@code =>} sequence number within primary key( a value
	 *  of 1 represents the first column of the primary key, a value of 2 would
	 *  represent the second column within the primary key).
	 *  <LI><B>PK_NAME</B> String {@code =>} primary key name (may be <code>null</code>)
	 *  </OL>
	 *
	 * @param catalog a catalog name; must match the catalog name as it
	 *        is stored in the database; "" retrieves those without a catalog;
	 *        <code>null</code> means that the catalog name should not be used to narrow
	 *        the search
	 * @param schema a schema name; must match the schema name
	 *        as it is stored in the database; "" retrieves those without a schema;
	 *        <code>null</code> means that the schema name should not be used to narrow
	 *        the search
	 * @param table a table name; must match the table name as it is stored
	 *        in the database
	 * @return <code>ResultSet</code> - each row is a primary key column description
	 * @exception SQLException if a database access error occurs
	 */
	ResultSet getPrimaryKeys(String catalog, String schema,
							 String table) throws SQLException;

	/**
	 * Retrieves a description of the given table's indices and statistics. They are
	 * ordered by NON_UNIQUE, TYPE, INDEX_NAME, and ORDINAL_POSITION.
	 *
	 * <P>Each index column description has the following columns:
	 *  <OL>
	 *  <LI><B>TABLE_CAT</B> String {@code =>} table catalog (may be <code>null</code>)
	 *  <LI><B>TABLE_SCHEM</B> String {@code =>} table schema (may be <code>null</code>)
	 *  <LI><B>TABLE_NAME</B> String {@code =>} table name
	 *  <LI><B>NON_UNIQUE</B> boolean {@code =>} Can index values be non-unique.
	 *      false when TYPE is tableIndexStatistic
	 *  <LI><B>INDEX_QUALIFIER</B> String {@code =>} index catalog (may be <code>null</code>);
	 *      <code>null</code> when TYPE is tableIndexStatistic
	 *  <LI><B>INDEX_NAME</B> String {@code =>} index name; <code>null</code> when TYPE is
	 *      tableIndexStatistic
	 *  <LI><B>TYPE</B> short {@code =>} index type:
	 *      <UL>
	 *      <LI> tableIndexStatistic - this identifies table statistics that are
	 *           returned in conjuction with a table's index descriptions
	 *      <LI> tableIndexClustered - this is a clustered index
	 *      <LI> tableIndexHashed - this is a hashed index
	 *      <LI> tableIndexOther - this is some other style of index
	 *      </UL>
	 *  <LI><B>ORDINAL_POSITION</B> short {@code =>} column sequence number
	 *      within index; zero when TYPE is tableIndexStatistic
	 *  <LI><B>COLUMN_NAME</B> String {@code =>} column name; <code>null</code> when TYPE is
	 *      tableIndexStatistic
	 *  <LI><B>ASC_OR_DESC</B> String {@code =>} column sort sequence, "A" {@code =>} ascending,
	 *      "D" {@code =>} descending, may be <code>null</code> if sort sequence is not supported;
	 *      <code>null</code> when TYPE is tableIndexStatistic
	 *  <LI><B>CARDINALITY</B> long {@code =>} When TYPE is tableIndexStatistic, then
	 *      this is the number of rows in the table; otherwise, it is the
	 *      number of unique values in the index.
	 *  <LI><B>PAGES</B> long {@code =>} When TYPE is  tableIndexStatisic then
	 *      this is the number of pages used for the table, otherwise it
	 *      is the number of pages used for the current index.
	 *  <LI><B>FILTER_CONDITION</B> String {@code =>} Filter condition, if any.
	 *      (may be <code>null</code>)
	 *  </OL>
	 *
	 * @param catalog a catalog name; must match the catalog name as it
	 *        is stored in this database; "" retrieves those without a catalog;
	 *        <code>null</code> means that the catalog name should not be used to narrow
	 *        the search
	 * @param schema a schema name; must match the schema name
	 *        as it is stored in this database; "" retrieves those without a schema;
	 *        <code>null</code> means that the schema name should not be used to narrow
	 *        the search
	 * @param table a table name; must match the table name as it is stored
	 *        in this database
	 * @param unique when true, return only indices for unique values;
	 *     when false, return indices regardless of whether unique or not
	 * @param approximate when true, result is allowed to reflect approximate
	 *     or out of data values; when false, results are requested to be
	 *     accurate
	 * @return <code>ResultSet</code> - each row is an index column description
	 * @exception SQLException if a database access error occurs
	 */
	ResultSet getIndexInfo(String catalog, String schema, String table,
						   boolean unique, boolean approximate)
		throws SQLException;
#endif

private:
	friend class EDBHandler;

	EString productName;
	EString productVersion;
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDATABASEMETADATA_HH_ */
