/*
 * EUpdateStatement.cpp
 *
 *  Created on: 2017-6-5
 *      Author: cxxjava@163.com
 */

#include "../inc/EUpdateStatement.hh"
#include "../inc/EConnection.hh"

namespace efc {
namespace edb {

static int get_affected_index(es_bson_node_t *node) {
	return atoi(node->name);
}

static int get_affected_count(es_bson_node_t *node) {
	char s1[30];
	eso_strsplit((char*)node->value->data, "?", 1, s1, sizeof(s1));
	return atoi(s1);
}

static char* get_affected_errmsg(es_bson_node_t *node) {
	if (!node) return null;
	char* p = eso_strnstr((char*)node->value->data, node->value->len, "?");
	return (char*)(p ? (p + 1) : "");
}

//=============================================================================

EUpdateStatement::~EUpdateStatement() {

}

EUpdateStatement::EUpdateStatement() :
		isFailedResume(false) {
}

EUpdateStatement::EUpdateStatement(EConnection* conn) :
		EStatement(conn), isFailedResume(false) {
}

void EUpdateStatement::setFailedResume(boolean resume) {
	isFailedResume = resume;
}

EStatement& EUpdateStatement::addBatch(const char* sql) {
	if (!sql) {
		throw ENullPointerException(__FILE__, __LINE__, "sql");
	}

	sqlStmt.add(EDB_KEY_SQLS "/" EDB_KEY_SQL, sql);
	sqlCount++;

	return *this;
}

EStatement& EUpdateStatement::addBatchFormat(const char* sqlfmt, ...) {
	va_list args;
	EString sql;

	va_start(args, sqlfmt);
	sql.vformat(sqlfmt, args);
	addBatch(sql.c_str());
	va_end(args);

	return *this;
}

EStatement& EUpdateStatement::addBatch() {
	sqlStmt.add(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS, NULL);
	sqlCount++;

	return *this;
}

int EUpdateStatement::getSqlCount() {
	return sqlCount;
}

EStatement& EUpdateStatement::execute(void) {
	if (!connection) {
		throw ESQLException(__FILE__, __LINE__, "connection lost");
	}

	if (!hasRequestHead) {
		connection->dbHandler.genRequestBase(&sqlStmt, DB_SQL_UPDATE);
		hasRequestHead = true;
	}
	sqlStmt.setByte(EDB_KEY_RESUME, isFailedResume ? 1 : 0);

	//请求并获取结果
	result = connection->dbHandler.executeSQL(&sqlStmt);

	if (!result || result->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, result->getString(EDB_KEY_ERRMSG).c_str());
	}

	return *this;
}

int EUpdateStatement::getFirstFailed(void) {
	if (!result) {
		return -1;
	}

	es_bson_node_t *node = result->find(EDB_KEY_RESULT)->child0;
	while (node) {
		if (get_affected_count(node) < 0) {
			return get_affected_index(node);
		}
		node = node->next;
	}

	return -1;
}

int EUpdateStatement::getFailures(void) {
	if (!result) {
		return -1;
	}

	int n;
	int count = 0;
	es_bson_node_t *node = result->find(EDB_KEY_RESULT)->child0;
	while (node) {
		if ((n = get_affected_count(node)) < 0) {
			count += n;
		}
		node = node->next;
	}

	return count;
}

int EUpdateStatement::getUpdateCount(int index) {
	if (!result || index < 1) {
		return -1;
	}

	es_bson_node_t *node = result->find(EDB_KEY_RESULT)->child0;
	while (node) {
		if (get_affected_index(node) == index) {
			break;
		}
		node = node->next;
	}
	if (node) {
		return get_affected_count(node);
	}
	return -1;
}

EString EUpdateStatement::getUpdateMessage(int index) {
	if (!result || index < 1) {
		return null;
	}

	es_bson_node_t *node = result->find(EDB_KEY_RESULT)->child0;
	while (node && (--index > 0)) {
		node = node->next;
	}
	return get_affected_errmsg(node);
}

EString EUpdateStatement::getErrorMessage(void) {
	return result->getString(EDB_KEY_ERRMSG, "");
}

void EUpdateStatement::clear() {
	result.reset();
}

} /* namespace edb */
} /* namespace efc */
