/*
 * EStatement.cpp
 *
 *  Created on: 2017-6-5
 *      Author: cxxjava@163.com
 */

#include "../inc/EStatement.hh"
#include "../inc/EConnection.hh"

namespace efc {
namespace edb {

EStatement::~EStatement() {
}

EStatement::EStatement() :
		connection(null), sqlCount(0), fetchSize(
				EConnection::DEFAULT_FETCH_SIZE), hasRequestHead(false) {
}

EStatement::EStatement(EConnection *conn) :
		connection(conn), sqlCount(0), fetchSize(
				EConnection::DEFAULT_FETCH_SIZE), hasRequestHead(false) {
}

void EStatement::setConnection(EConnection *conn) {
	connection = conn;
}

EConnection* EStatement::getConnection() {
	return connection;
}

EStatement& EStatement::setSql(const char *sql) {
	if (!sql || !*sql) {
		throw ENullPointerException(__FILE__, __LINE__, "sql is null");
	}

	//clear old
	clearSql();

	//add new
	sqlStmt.add(EDB_KEY_SQLS "/" EDB_KEY_SQL, sql);
	sqlCount++;

	return *this;
}

EStatement& EStatement::setSqlFormat(const char *sqlfmt, ...) {
	va_list args;
	EString sql;

	va_start(args, sqlfmt);
	sql.vformat(sqlfmt, args);
	try {
		setSql(sql.c_str());
	} catch (EThrowable& t) {
		va_end(args);
		throw t;
	}
	va_end(args);

	return *this;
}

EStatement& EStatement::clearSql() {
	sqlStmt.clear();
	sqlCount = 0;
	hasRequestHead = false;
	return *this;
}

EStatement& EStatement::bind(edb_field_type_e fieldType,
				 const void *value, int size,
				 int fieldMaxSize) {
	//<param>4字节的数据类型 + 4字节的最大字段长度 + 4字节绑定数据长度 + N字节的绑定数据</param>
	EByteArrayOutputStream baos;
	EDataOutputStream dos(&baos);
	dos.writeInt(fieldType);
	dos.writeInt(fieldMaxSize);
	dos.writeInt(size);
	if (value) {
		dos.write(value, size);
	}

	//param
	sqlStmt.add(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM, baos.data(), baos.size());

	return *this;
}

EStatement& EStatement::bind(edb_field_type_e fieldType, const char *value) {
	return bind(fieldType, value, value ? strlen(value) : 0);
}

EStatement& EStatement::bindFormat(edb_field_type_e fieldType, const char *fmt, ...) {
	if (!fmt || !*fmt) {
		throw ENullPointerException(__FILE__, __LINE__, "fmt is null");
	}

	va_list args;
	es_string_t *value = NULL;

	va_start(args, fmt);
	eso_mvsprintf(&value, fmt, args);
	va_end(args);

	bind(fieldType, value, strlen(value), 0);
	eso_mfree(&value);

	return *this;
}

EStatement& EStatement::bindString(const char* value) {
	return bind(DB_dtString, value, value ? strlen(value) : 0);
}

EStatement& EStatement::bindBool(boolean value) {
	return bind(DB_dtBool, value ? "1" : "0");
}

EStatement& EStatement::bindBool(const char* value) {
	return bind(DB_dtBool, value);
}

EStatement& EStatement::bindShort(short value) {
	EString s(value);
	return bind(DB_dtShort, s.c_str());
}

EStatement& EStatement::bindShort(const char* value) {
	return bind(DB_dtShort, value);
}

EStatement& EStatement::bindInt(int value) {
	EString s(value);
	return bind(DB_dtInt, s.c_str());
}

EStatement& EStatement::bindInt(const char* value) {
	return bind(DB_dtInt, value);
}

EStatement& EStatement::bindLong(llong value) {
	EString s(value);
	return bind(DB_dtLong, s.c_str());
}

EStatement& EStatement::bindLong(const char* value) {
	return bind(DB_dtLong, value);
}

EStatement& EStatement::bindFloat(float value) {
	return bindDouble(value);
}

EStatement& EStatement::bindFloat(const char* value) {
	return bindDouble(value);
}

EStatement& EStatement::bindDouble(double value) {
	llong lv = EDouble::doubleToLLongBits(value);
	es_byte_t s[8] = {0};
	eso_llong2array(lv, s, 8);
	return bind(DB_dtReal, s, 8);
}

EStatement& EStatement::bindDouble(const char* value) {
	double d = EDouble::parseDouble(value);
	return bindDouble(d);
}

EStatement& EStatement::bindNumeric(llong value) {
	EString s((llong)value);
	return bind(DB_dtNumeric, s.c_str());
}

EStatement& EStatement::bindNumeric(const char* value) {
	return bind(DB_dtNumeric, value);
}

EStatement& EStatement::bindNumeric(EBigInteger& value) {
	return bind(DB_dtNumeric, value.toString().c_str());
}

EStatement& EStatement::bindNumeric(EBigDecimal& value) {
	return bind(DB_dtNumeric, value.toString().c_str());
}

EStatement& EStatement::bindDateTime(EDate* value) {
	if (!value) {
		throw ENullPointerException(__FILE__, __LINE__, "value is null");
	}
	return bind(DB_dtDateTime, value->toString().c_str());
}

EStatement& EStatement::bindDateTime(const char* value) {
	return bind(DB_dtDateTime, value);
}

EStatement& EStatement::bindBytes(byte* value, int size) {
	return bind(DB_dtBytes, value, size);
}

EStatement& EStatement::bindNull() {
	return bind(DB_dtNull, NULL, 0);
}

void EStatement::setFetchSize(int rows) {
	fetchSize = rows;
}

int EStatement::getFetchSize() {
	return fetchSize;
}

} /* namespace edb */
} /* namespace efc */
