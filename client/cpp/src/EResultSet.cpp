/*
 * EResultSet.cpp
 *
 *  Created on: 2017-6-6
 *      Author: cxxjava@163.com
 */

#include "../inc/EResultSet.hh"
#include "../inc/EConnection.hh"

namespace efc {
namespace edb {

EResultSet::EResultSet(EStatement* stmt, sp<EBson> rep):
		statement(stmt),
		dataset(rep),
		metaData(rep),
		currRow(null),
		rows(0) {
	cursorID = rep->getLLong(EDB_KEY_DATASET "/" EDB_KEY_CURSOR);
}

boolean EResultSet::next() {
	if (!currRow) {
		currRow = dataset->find(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD);
	} else {
		currRow = currRow->next;
	}

	boolean eof = (dataset->getByte(EDB_KEY_DATASET "/" EDB_KEY_RECORDS) != 0);
	if (!currRow && !eof) {
		//cursor fetch next.
		fetch();

		//reset currrow
		currRow = dataset->find(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD);
	}

	rows++;

	return !!currRow;
}

void EResultSet::close() {
	//关闭后端结果集
	EConnection *connection = statement->getConnection();
	EBson req;
	connection->dbHandler.genRequestBase(&req, DB_SQL_RESULT_CLOSE);
	req.addLLong(EDB_KEY_CURSOR, cursorID);

	//请求并获取结果
	sp<EBson> rep = connection->dbHandler.resultClose(&req);
	if (!rep || rep->getInt(EDB_KEY_ERRCODE) != 0) {
		throw ESQLException(__FILE__, __LINE__, "result close");
	}

	//本地dataset清理
	dataset.reset();
}

EResultSetMetaData* EResultSet::getMetaData() {
	return &metaData;
}

void EResultSet::fetch() {
	EConnection *connection = statement->getConnection();
	EBson req;
	connection->dbHandler.genRequestBase(&req, DB_SQL_RESULT_FETCH);
	req.addLLong(EDB_KEY_CURSOR, cursorID);

	//请求并获取结果
	dataset = connection->dbHandler.resultFetch(&req);
}

int EResultSet::findColumn(const char *columnLabel) {
	es_bson_node_t *node = metaData.fields->find(EDB_KEY_FIELDS "/" EDB_KEY_FIELD);
	if (!node) {
		return 0;
	}
	int index = 1;
	while (node) {
		es_bson_node_t *nn = eso_bson_node_child_get(node, EDB_KEY_NAME);
		if (EBson::nodeGetString(nn).equalsIgnoreCase(columnLabel)) {
			return index;
		}
		node = node->next;
		index++;
	}
	return 0;
}

int EResultSet::getRow() {
	return rows;
}

void* EResultSet::getColumnData(int columnIndex, es_size_t *size) {
	if (columnIndex < 1 || columnIndex > metaData.getColumnCount()) {
		throw EIndexOutOfBoundsException(__FILE__, __LINE__);
	}

	int skiped = 0;
	es_size_t dsize;
	void *data = dataset->nodeGet(currRow, &dsize);
	EByteArrayInputStream bais(data, dsize);
	EDataInputStream dis(&bais);
	for (int i = 0; i < columnIndex - 1; i++) {
		int l = dis.readInt();
		if (l == -1) { // is null.
			skiped += 4;
			dis.skipBytes(0);
		} else {
			skiped += 4 + l;
			dis.skipBytes(l);
		}
	}
	*size = dis.readInt();
	if (*size == -1) { // is null.
		*size = 0;
		return NULL;
	} else {
		return (char*)data + skiped + 4;
	}
}

boolean EResultSet::isNull(int columnIndex) {
	es_size_t size;
	char *val = (char*)getColumnData(columnIndex, &size);
	return (!val && size == 0);
}

EString EResultSet::getString(int columnIndex) {
	es_size_t size;
	char *val = (char*)getColumnData(columnIndex, &size);
	return EString(val, 0, size);
}

boolean EResultSet::getBoolean(int columnIndex) {
	return EBoolean::parseBoolean(getString(columnIndex).c_str());
}

byte EResultSet::getByte(int columnIndex) {
	return EByte::parseByte(getString(columnIndex).c_str());
}

short EResultSet::getShort(int columnIndex) {
	return EShort::parseShort(getString(columnIndex).c_str());
}

int EResultSet::getInt(int columnIndex) {
	return EInteger::parseInt(getString(columnIndex).c_str());
}

llong EResultSet::getLLong(int columnIndex) {
	return ELLong::parseLLong(getString(columnIndex).c_str());
}

float EResultSet::getFloat(int columnIndex) {
	return EFloat::parseFloat(getString(columnIndex).c_str());
}

double EResultSet::getDouble(int columnIndex) {
	return EDouble::parseDouble(getString(columnIndex).c_str());
}

EA<byte> EResultSet::getBytes(int columnIndex) {
	es_size_t size;
	byte *val = (byte*)getColumnData(columnIndex, &size);
	return EA<byte>(val, size);
}

EDate EResultSet::getDate(int columnIndex) {
	return ECalendar::parseOf("%Y-%m-%d %H:%M:%S", getString(columnIndex).c_str()).getTime();
}

EBigDecimal EResultSet::getBigDecimal(int columnIndex) {
	return getString(columnIndex).c_str();
}

sp<EInputStream> EResultSet::getAsciiStream(int columnIndex) {
	//FIXME:
	return getBinaryStream(columnIndex);
}

sp<EInputStream> EResultSet::getBinaryStream(int columnIndex) {
	es_size_t size;
	byte *val = (byte*)getColumnData(columnIndex, &size);
	return new EByteArrayInputStream(val, size);
}

boolean EResultSet::isNull(const char* columnLabel) {
	return isNull(findColumn(columnLabel));
}

EString EResultSet::getString(const char* columnLabel) {
	return getString(findColumn(columnLabel));
}

boolean EResultSet::getBoolean(const char* columnLabel) {
	return getBoolean(findColumn(columnLabel));
}

byte EResultSet::getByte(const char* columnLabel) {
	return getByte(findColumn(columnLabel));
}

short EResultSet::getShort(const char* columnLabel) {
	return getShort(findColumn(columnLabel));
}

int EResultSet::getInt(const char* columnLabel) {
	return getInt(findColumn(columnLabel));
}

llong EResultSet::getLLong(const char* columnLabel) {
	return getLLong(findColumn(columnLabel));
}

float EResultSet::getFloat(const char* columnLabel) {
	return getFloat(findColumn(columnLabel));
}

double EResultSet::getDouble(const char* columnLabel) {
	return getDouble(findColumn(columnLabel));
}

EA<byte> EResultSet::getBytes(const char* columnLabel) {
	return getBytes(findColumn(columnLabel));
}

EDate EResultSet::getDate(const char* columnLabel) {
	return getDate(findColumn(columnLabel));
}

EBigDecimal EResultSet::getBigDecimal(const char* columnLabel) {
	return getBigDecimal(findColumn(columnLabel));
}

sp<EInputStream> EResultSet::getAsciiStream(const char* columnLabel) {
	return getAsciiStream(findColumn(columnLabel));
}

sp<EInputStream> EResultSet::getBinaryStream(const char* columnLabel) {
	return getBinaryStream(findColumn(columnLabel));
}

} /* namespace edb */
} /* namespace efc */
