/*
 * EResultSetMetaData.cpp
 *
 *  Created on: 2017-6-7
 *      Author: cxxjava@163.com
 */

#include "../inc/EResultSetMetaData.hh"

namespace efc {
namespace edb {

EResultSetMetaData::EResultSetMetaData(sp<EBson> rep):
		fields(new EBson()) {
	fields->copyFrom(NULL, rep.get(), EDB_KEY_DATASET "/" EDB_KEY_FIELDS);
	columns = fields->count(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
}

int	EResultSetMetaData::getColumnCount() {
	return columns;
}

EString EResultSetMetaData::getColumnLabel(int column) {
	EString path(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	path.append("|").append(column).append("/").append(EDB_KEY_NAME);
	return fields->getString(path.c_str());
}

int EResultSetMetaData::getPrecision(int column) {
	EString path(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	path.append("|").append(column).append("/").append(EDB_KEY_PRECISION);
	return fields->getInt(path.c_str());
}

int EResultSetMetaData::getScale(int column) {
	EString path(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	path.append("|").append(column).append("/").append(EDB_KEY_SCALE);
	return fields->getInt(path.c_str());
}

edb_field_type_e EResultSetMetaData::getColumnType(int column) {
	EString path(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	path.append("|").append(column).append("/").append(EDB_KEY_TYPE);
	return (edb_field_type_e)fields->getInt(path.c_str());
}

int EResultSetMetaData::getColumnDisplaySize(int column) {
	EString path(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	path.append("|").append(column).append("/").append(EDB_KEY_LENGTH);
	return fields->getInt(path.c_str());
}

EStringBase EResultSetMetaData::toString() {
	return fields->toString();
}

} /* namespace edb */
} /* namespace efc */
