/*
 * EDatabase_mysql.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "./EDatabase_mysql.hh"

namespace efc {
namespace edb {

static int stmt_send_long_data(MYSQL_STMT *stmt,
        int param_count,
		MYSQL_BIND *params,
		byte *paramIsStream,
        sp<EIterator<EInputStream*> > iter) {
	char buf[8192];
	int len;
	my_bool ret;
	for (int i=0; i<param_count; i++) {
		if (params[i].buffer_length == 0 && paramIsStream[i] == 1 && iter->hasNext()) {
			EInputStream* is = iter->next();
			while ((len = is->read(buf, sizeof(buf))) > 0) {
				if ((ret = mysql_stmt_send_long_data(stmt, i, buf, len)) != 0) {
					return ret;
				}
			}
		}
	}
	return 0;
}

//=============================================================================

EDatabase_mysql::Stmt::~Stmt() {
	delete fields;
	delete lengths;
}

EDatabase_mysql::Stmt::Stmt(Result* result, MYSQL_STMT* stmt, int cols): stmt(stmt), bufs(cols) {
	fields = new MYSQL_BIND[cols]{};
	lengths = new unsigned long[cols]{};
	for (int i = 0; i < cols; i++) {
		Result::Field* field = result->getField(i);
		bufs[i] = new EA<byte>(field->length + 1);
		fields[i].buffer_type = MYSQL_TYPE_STRING;
		fields[i].buffer = bufs[i]->address();
		fields[i].buffer_length = bufs[i]->length() - 1;
		fields[i].length = &lengths[i];
	}

	mysql_stmt_bind_result(stmt, fields);
	mysql_stmt_store_result(stmt);
}

EDatabase_mysql::Result::~Result() {
	clear();
}

EDatabase_mysql::Result::Result(MYSQL_RES* result, MYSQL_STMT* stmt, int fetchsize, int row_offset, int rows_size):
		fetch_size(fetchsize),
		row_offset(row_offset),
		rows_size(rows_size),
		current_row(0),
		result(result),
		mystmt(null),
		fields(null),
		cols(0) {
	ES_ASSERT(result);

	//fieldcount
	cols = mysql_num_fields(result);
	//fields
	fields = new Field[cols];
	//rows
	//if mysql_unbuttered_query() then can't use mysql_num_rows().

	MYSQL_FIELD	*mysql_field;
	for (int i = 0; i < cols; i++) {
		mysql_field = mysql_fetch_field(result);
		char* name = mysql_field->name;
		enum enum_field_types type = mysql_field->type;
		unsigned int length = mysql_field->length;
		unsigned int decimals = mysql_field->decimals;
		unsigned int flags = mysql_field->flags;
		int precision = 0; // calculated by CnvtNativeToStd
		edb_field_type_e eDataType = CnvtNativeToStd(type, length, precision, decimals, flags);
		//字段名,类型,长度,有效位数,比例|
		fields[i].name = name;
		fields[i].navite_type = type;
		fields[i].length = length;
		fields[i].precision = precision;
		fields[i].scale = decimals;
		fields[i].type = eDataType;
	}

	if (stmt) {
		mystmt = new Stmt(this, stmt, cols);
	}
}

MYSQL_RES* EDatabase_mysql::Result::getResult() {
	return result;
}

EDatabase_mysql::Stmt* EDatabase_mysql::Result::getStatement() {
	return mystmt;
}

void EDatabase_mysql::Result::clear() {
	delete mystmt;

	if (result) {
		mysql_free_result(result);
		result = NULL;
	}
	if (fields) {
		delete[] fields;
		fields = NULL;
	}
	current_row = cols = 0;
}

int EDatabase_mysql::Result::getCols() {
	return cols;
}

EDatabase_mysql::Result::Field* EDatabase_mysql::Result::getField(int col) {
	if (col >= cols) {
		return NULL;
	}
	return &fields[col];
}

//=============================================================================

void EDatabase_mysql::resetStatement() {
	if (m_Stmt) {
		mysql_stmt_free_result(m_Stmt);
		mysql_stmt_close(m_Stmt);
	}
	m_Stmt = NULL;
}

boolean EDatabase_mysql::createDataset(EBson *rep, int fetchsize, int offset,
		int rows, boolean newResult) {
	//dataset
	rep->add(EDB_KEY_DATASET, NULL);

	int nFields = m_Result->getCols();

	if (newResult) {
		//fields
		for (int i = 0; i < nFields; i++) {
			Result::Field* field = m_Result->getField(i);
			rep->add(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD, NULL);
			rep->add(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD "/" EDB_KEY_NAME, field->name.c_str());
			rep->addInt(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD "/" EDB_KEY_TYPE, field->type);
			rep->addInt(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD "/" EDB_KEY_LENGTH, field->length);
			rep->addInt(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD "/" EDB_KEY_PRECISION, field->precision);
			rep->addInt(EDB_KEY_DATASET "/" EDB_KEY_FIELDS "/" EDB_KEY_FIELD "/" EDB_KEY_SCALE, field->scale);
		}
		//cursor
		rep->addInt(EDB_KEY_DATASET "/" EDB_KEY_CURSOR, newCursorID());
	}

	//records
	boolean eof = true;
	int count = 0;

	MYSQL_RES* result = m_Result->getResult();
	Stmt* mystmt = m_Result->getStatement();

	if (mystmt) {
		MYSQL_STMT *stmt = mystmt->stmt;
		MYSQL_BIND *fields = mystmt->fields;
		unsigned long *lengths = mystmt->lengths;

		while (mysql_stmt_fetch(stmt) == 0) {
			m_Result->current_row++;

			if (offset >= m_Result->current_row) //起始行
				continue;

			if (rows != 0 && (m_Result->current_row - offset) > rows) { //总记录数
				break;
			}

			//record
			EByteArrayOutputStream baos;
			EDataOutputStream dos(&baos);
			for (int i = 0; i < nFields; i++) {
				char *value = (char*)fields[i].buffer;
				int length = (int)lengths[i];
				if (length == 0) {
					dos.writeInt(-1); //is null
				} else {
					dos.writeInt(length);
					dos.write(value, length);
				}
			}
			rep->add(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD, baos.data(), baos.size());

			if (++count >= fetchsize) {
				eof = false;
				break;
			}
		}

	} else { //!

		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			m_Result->current_row++;

			if (offset >= m_Result->current_row) //起始行
				continue;

			if (rows != 0 && (m_Result->current_row - offset) > rows) { //总记录数
				break;
			}

			//record
			EByteArrayOutputStream baos;
			EDataOutputStream dos(&baos);
			unsigned long *lengths = mysql_fetch_lengths(result);
			for (int i = 0; i < nFields; i++) {
				char *value = row[i];
				unsigned long length = lengths[i];
				if (!value) {
					dos.writeInt(-1); //is null
				} else {
					dos.writeInt(length);
					dos.write(value, length);
				}
			}
			rep->add(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD, baos.data(), baos.size());

			if (++count >= fetchsize) {
				eof = false;
				break;
			}
		}
	}

	rep->setByte(EDB_KEY_DATASET "/" EDB_KEY_RECORDS, eof ? 1 : 0);  //更新EOF标记
	return true;
}

EDatabase_mysql::~EDatabase_mysql() {
	close();
}

EDatabase_mysql::EDatabase_mysql(EDBProxyInf* proxy) :
		EDatabase(proxy),
		m_Conn(null), m_Stmt(null) {
}

EString EDatabase_mysql::dbtype() {
	return  EString("MYSQL");
}

EString EDatabase_mysql::dbversion() {
	return  EString(mysql_get_server_info(m_Conn));
}

boolean EDatabase_mysql::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset, int timeout) {
	//init.
	mysql_init(&mysql);
	m_Conn = &mysql;

	unsigned int tm = timeout;
	mysql_options(m_Conn, MYSQL_OPT_CONNECT_TIMEOUT, &tm);
	mysql_options(m_Conn, MYSQL_OPT_READ_TIMEOUT, &tm);
	mysql_options(m_Conn, MYSQL_OPT_WRITE_TIMEOUT, &tm);

	if (!(mysql_real_connect(m_Conn, host, username, password, database,
			port/*MYSQL_PORT*/, NULL, CLIENT_MULTI_RESULTS))) {
		setErrorMessage(mysql_error(m_Conn));
		mysql_close(m_Conn);
		return false;
	}

	if (charset && strlen(charset) > 0) {
		if (mysql_set_character_set(m_Conn, charset) != 0) {
			setErrorMessage(mysql_error(m_Conn));
			mysql_close(m_Conn);
			return false;
		}
	}

	//默认开启自动提交模式
	setAutoCommit(true);

	return true;
}

boolean EDatabase_mysql::close() {
	resetStatement();
	m_Result.reset();

	//若启用了事务则默认进行回滚
	if (m_Conn) {
		if (!m_AutoCommit) {
			mysql_rollback(m_Conn);
		}
		mysql_close(m_Conn);
	}
	m_Conn = NULL;

	return true;
}

sp<EBson> EDatabase_mysql::onExecute(EBson *req, EIterable<EInputStream*>* itb) {
	int fetchsize = req->getInt(EDB_KEY_FETCHSIZE, 4096);
	char *sql = req->get(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	es_bson_node_t *param_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
	MYSQL_RES* result;

	resetStatement();
	m_Result.reset();

	dumpSQL(sql);

	if (!param_node) {
		if (mysql_real_query(m_Conn, sql, strlen(sql)) != 0) {
			setErrorMessage(mysql_error(m_Conn));
			return null;
		}

		result = mysql_use_result(m_Conn);
		if (result) {
			//result reset.
			m_Result = new Result(result, NULL, fetchsize, 0, 0);

			//resp pkg
			sp<EBson> rep = new EBson();
			createDataset(rep.get(), fetchsize, 0, 0, true);
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

			//affected
			rep->addLLong(EDB_KEY_AFFECTED, mysql_affected_rows(m_Conn));

			return rep;
		} else if (mysql_errno(m_Conn) == 0) {
			sp<EBson> rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
			//affected
			rep->addLLong(EDB_KEY_AFFECTED, mysql_affected_rows(m_Conn));
			return rep;
		}
	} else {
		MYSQL_STMT *stmt;

		if (!(stmt = mysql_stmt_init(m_Conn))) {
			setErrorMessage(mysql_error(m_Conn));
			return null;
		}

		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			setErrorMessage(mysql_stmt_error(stmt));
			mysql_stmt_close(stmt);
			return null;
		}

		int param_count = req->count(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
		int param_count2= mysql_stmt_param_count(stmt);

		if (param_count != param_count2) {
			setErrorMessage("params error");
			mysql_stmt_close(stmt);
			return null;
		}

		MYSQL_BIND params[param_count];
		memset(params, 0, sizeof(params));
		my_bool paramIsnull[param_count];
		byte paramIsStream[param_count];

		int index = 0;
		while (param_node) {
			es_size_t size = 0;
			char *value = (char*)EBson::nodeGet(param_node, &size);
			ES_ASSERT(value && size);

			edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);
			int maxsize = EStream::readInt(value + 4);
			int datalen = EStream::readInt(value + 8);
			char *binddata = value + 12;

			if (stdtype == DB_dtReal && datalen == 8) {
				llong l = eso_array2llong((es_byte_t*)binddata, datalen);
				double d = EDouble::llongBitsToDouble(l);
				memcpy(binddata, &d, datalen);
			}

			paramIsnull[index] = (stdtype == DB_dtNull) ? 1 : 0;
			paramIsStream[index] = (datalen == -1) ? 1 : 0;

			switch (stdtype) {
			case DB_dtReal:
				params[index].buffer_type = FIELD_TYPE_DOUBLE;
				break;
			case DB_dtBLob:
			case DB_dtBytes:
			case DB_dtLongBinary:
				params[index].buffer_type = FIELD_TYPE_BLOB;
				break;
			default:
				params[index].buffer_type = FIELD_TYPE_STRING;
				break;
			}
			params[index].buffer = datalen ? binddata : NULL;
			params[index].buffer_length = datalen > 0 ? datalen : 0;
			params[index].is_null = &paramIsnull[index];
			params[index].length = 0;

			//next
			param_node = param_node->next;
			index++;
		}

		if (mysql_stmt_bind_param(stmt, params) != 0
				|| (itb && stmt_send_long_data(stmt, param_count, params, paramIsStream, itb->iterator()) != 0)
				|| mysql_stmt_execute(stmt) != 0) {
			setErrorMessage(mysql_stmt_error(stmt));
			mysql_stmt_close(stmt);
			return null;
		}

		result = mysql_stmt_result_metadata(stmt);
		if (result) {
			//result reset.
			m_Stmt = stmt;
			m_Result = new Result(result, stmt, fetchsize, 0, 0);

			//resp pkg
			sp<EBson> rep = new EBson();
			createDataset(rep.get(), fetchsize, 0, 0, true);
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

			//affected
			rep->addLLong(EDB_KEY_AFFECTED, mysql_stmt_affected_rows(stmt));

			return rep;
		} else if (mysql_stmt_errno(stmt) == 0) {
			sp<EBson> rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
			//affected
			rep->addLLong(EDB_KEY_AFFECTED, mysql_stmt_affected_rows(stmt));
			mysql_stmt_close(stmt);
			return rep;
		}

		mysql_stmt_close(stmt);
	}

	setErrorMessage(mysql_error(m_Conn));
	return null;
}

static void addAffected(EBson* rep, int index, int affected, const char* errmsg=NULL) {
	EString path(EDB_KEY_RESULT "/");
	path.append(index); // index
	if (!errmsg) {
		EString s(affected);
		rep->add(path.c_str(), s.c_str());
	} else {
		EByteBuffer bb;
		bb.appendFormat("%d?%s", affected, errmsg);
		rep->add(path.c_str(), bb.data(), bb.size());
	}
}
#define AFFECTED_SUCCESS(affected) do { addAffected(rep.get(), sqlIndex, affected, NULL); } while (0);
#define AFFECTED_FAILURE(count, errmsg) do { addAffected(rep.get(), sqlIndex, count, errmsg); hasFailed = true; } while (0);

sp<EBson> EDatabase_mysql::onUpdate(EBson *req, EIterable<EInputStream*>* itb) {
	es_bson_node_t *sql_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	boolean isResume = req->getByte(EDB_KEY_RESUME, 0);
	boolean hasFailed = false;
	int sqlIndex = 0;

	resetStatement();
	m_Result.reset();

	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

	while (sql_node) {
		if (!isResume && hasFailed) {
			break; //abort!
		}

		char *sql = EBson::nodeGet(sql_node);
		es_bson_node_t *params_node = EBson::childFind(sql_node, EDB_KEY_PARAMS);

		dumpSQL(sql);

		if (!params_node) {
			sqlIndex++;

			// real query
			if (mysql_real_query(m_Conn, sql, strlen(sql)) == 0) {
				AFFECTED_SUCCESS(mysql_affected_rows(m_Conn));
			} else {
				AFFECTED_FAILURE(-1, mysql_error(m_Conn));
			}

		} else { //!

			int batch_count = EBson::childCount(params_node, NULL);

			MYSQL_STMT *stmt;

			if ((stmt = mysql_stmt_init(m_Conn)) == NULL) {
				sqlIndex++;
				AFFECTED_FAILURE(-batch_count, mysql_error(m_Conn));
				sqlIndex += (batch_count - 1);
				sql_node = sql_node->next; //next
				continue;
			}

			if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
				sqlIndex++;
				AFFECTED_FAILURE(-batch_count,  mysql_stmt_error(stmt));
				sqlIndex += (batch_count - 1);
				mysql_stmt_close(stmt);
				sql_node = sql_node->next;
				continue;
			}

			int param_count = mysql_stmt_param_count(stmt);

			while (params_node) {
				sqlIndex++;

				if (!isResume && hasFailed) {
					break; //abort!
				}

				es_bson_node_t *param_node = EBson::childFind(params_node, EDB_KEY_PARAM);
				int param_count2 = EBson::childCount(params_node, EDB_KEY_PARAM);

				if (param_count != param_count2) {
					AFFECTED_FAILURE(-1,  "bind params error.");
					params_node = params_node->next; //next
					continue;
				}

				MYSQL_BIND params[param_count];
				memset(params, 0, sizeof(params));
				my_bool paramIsnull[param_count];
				byte paramIsStream[param_count];

				int index = 0;
				while (param_node) {
					es_size_t size = 0;
					char *value = (char*)EBson::nodeGet(param_node, &size);
					ES_ASSERT(value && size);

					edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);
					int maxsize = EStream::readInt(value + 4);
					int datalen = EStream::readInt(value + 8);
					char *binddata = value + 12;

					if (stdtype == DB_dtReal && datalen == 8) {
						llong l = eso_array2llong((es_byte_t*)binddata, datalen);
						double d = EDouble::llongBitsToDouble(l);
						memcpy(binddata, &d, datalen);
					}

					paramIsnull[index] = (stdtype == DB_dtNull) ? 1 : 0;
					paramIsStream[index] = (datalen == -1) ? 1 : 0;

					switch (stdtype) {
					case DB_dtReal:
						params[index].buffer_type = FIELD_TYPE_DOUBLE;
						break;
					case DB_dtBLob:
					case DB_dtBytes:
					case DB_dtLongBinary:
						params[index].buffer_type = FIELD_TYPE_BLOB;
						break;
					default:
						params[index].buffer_type = FIELD_TYPE_STRING;
						break;
					}
					params[index].buffer = datalen ? binddata : NULL;
					params[index].buffer_length = datalen > 0 ? datalen : 0;
					params[index].is_null = &paramIsnull[index];
					params[index].length = 0;

					//next
					param_node = param_node->next;
					index++;
				}

				//affected
				if (mysql_stmt_bind_param(stmt, params) != 0
						|| (itb && stmt_send_long_data(stmt, param_count, params, paramIsStream, itb->iterator()) != 0)
						|| mysql_stmt_execute(stmt) != 0) {
					AFFECTED_FAILURE(-1,  mysql_stmt_error(stmt));
				} else {
					AFFECTED_SUCCESS(mysql_stmt_affected_rows(stmt));
				}

				//next params
				params_node = params_node->next;
			}

			mysql_stmt_close(stmt);
		}

		//next sql.
		sql_node = sql_node->next;
	}

	return rep;
}

sp<EBson> EDatabase_mysql::onMoreResult(EBson *req) {
	int fetchsize = (m_Result != null) ? m_Result->fetch_size : 8192;
	int affected = 0;

	llong cursorID = req->getLLong(EDB_KEY_CURSOR);
	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}

	//free old.
	m_Result.reset();

	MYSQL_RES* result = null;
	if (m_Stmt) {
		if (mysql_stmt_next_result(m_Stmt) == 0) {
			result = mysql_stmt_result_metadata(m_Stmt);
			affected = mysql_stmt_affected_rows(m_Stmt);
			m_Result = new Result(result, m_Stmt, fetchsize, 0, 0);
		}
	} else {
		if ((mysql_next_result(m_Conn) == 0) && (result = mysql_use_result(m_Conn))) {
			affected = mysql_affected_rows(m_Conn);
			m_Result = new Result(result, NULL, fetchsize, 0, 0);
		}
	}

	sp<EBson> rep = new EBson();
	if (m_Result != null) {
		createDataset(rep.get(), fetchsize, 0, 0, true);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
		rep->addLLong(EDB_KEY_AFFECTED, affected);
	} else { // no more
		resetStatement();
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	}
	return rep;
}

sp<EBson> EDatabase_mysql::onResultFetch(EBson *req) {
	llong cursorID = req->getLLong(EDB_KEY_CURSOR);

	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}

	//resp pkg
	sp<EBson> rep = new EBson();
	createDataset(rep.get(), m_Result->fetch_size, m_Result->row_offset, m_Result->rows_size, false);
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

	return rep;
}

sp<EBson> EDatabase_mysql::onResultClose(EBson *req) {
	resetStatement();
	m_Result.reset();

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_mysql::onCommit() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	if (mysql_commit(m_Conn) != 0) {
		setErrorMessage(mysql_error(m_Conn));
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_mysql::onRollback() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	if (mysql_rollback(m_Conn) != 0) {
		setErrorMessage(mysql_error(m_Conn));
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_mysql::setAutoCommit(boolean autoCommit) {
	if (mysql_autocommit(m_Conn, autoCommit ? 1 : 0) != 0) {
		setErrorMessage(mysql_error(m_Conn));
		return null;
	}
	this->m_AutoCommit = autoCommit;

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_mysql::onLOBCreate() {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_mysql::onLOBWrite(llong oid, EInputStream *is) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_mysql::onLOBRead(llong oid, EOutputStream *os) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

edb_field_type_e EDatabase_mysql::CnvtNativeToStd(enum enum_field_types type,
        unsigned int length,
        int &precision,
        unsigned int decimals,
        unsigned int flags)
{
	edb_field_type_e eDataType;
	precision = length;	// can be adjusted

	switch(type)
	{
		case FIELD_TYPE_TINY:	// TINYINT field
		case FIELD_TYPE_SHORT:	// SMALLINT field
			eDataType = DB_dtShort;
			break;
		case FIELD_TYPE_INT24:	// MEDIUMINT field
			eDataType = DB_dtInt;
			break;
		case FIELD_TYPE_LONG:	// INTEGER field
			eDataType = DB_dtLong;
			break;
		case FIELD_TYPE_LONGLONG:	// BIGINT field
			eDataType = DB_dtNumeric;
			break;
		case FIELD_TYPE_DECIMAL:	// DECIMAL or NUMERIC field
			if (!(flags & UNSIGNED_FLAG))
				--precision;	// space for '-' sign
			if (decimals == 0) {
				// check for exact type
				if (precision < 5)
					eDataType = DB_dtShort;
				else if (precision < 10)
					eDataType = DB_dtLong;
				else
					eDataType = DB_dtNumeric;
			} else {
				--precision;	// space for '.'
				eDataType = DB_dtNumeric;
			}
			break;
		case FIELD_TYPE_FLOAT:	// FLOAT field
		case FIELD_TYPE_DOUBLE:	// DOUBLE or REAL field
			eDataType = DB_dtReal;
			break;
		case FIELD_TYPE_TIMESTAMP:	// TIMESTAMP field
		case FIELD_TYPE_DATE:	// DATE field
		case FIELD_TYPE_NEWDATE:
		case FIELD_TYPE_TIME:	// TIME field
		case FIELD_TYPE_DATETIME:	// DATETIME field
		case FIELD_TYPE_YEAR:	// YEAR field
			eDataType = DB_dtDateTime;
			break;
		case FIELD_TYPE_STRING:	// String (CHAR or VARCHAR) field, also SET and ENUM
		case FIELD_TYPE_VAR_STRING:
			if(flags & BINARY_FLAG)
				eDataType = DB_dtBytes;
			else
				eDataType = DB_dtString;
			break;
		case FIELD_TYPE_TINY_BLOB:
		case FIELD_TYPE_MEDIUM_BLOB:
		case FIELD_TYPE_LONG_BLOB:
		case FIELD_TYPE_BLOB:	// BLOB or TEXT field (use max_length to determine the maximum length)
			if(flags & BINARY_FLAG)
				eDataType = DB_dtBLob;
			else
				eDataType = DB_dtCLob;
			break;
		case FIELD_TYPE_SET:	// SET field
			eDataType = DB_dtString;
			break;
		case FIELD_TYPE_ENUM:	// ENUM field
			eDataType = DB_dtString;
			break;
		case FIELD_TYPE_NULL:	// NULL-type field
			eDataType = DB_dtString;
			break;

		default:
			eDataType = DB_dtString;
			break;
	}

	return eDataType;
}

} /* namespace edb */
} /* namespace efc */

//=============================================================================

extern "C" {
	ES_DECLARE(efc::edb::EDatabase*) makeDatabase(efc::edb::EDBProxyInf* proxy)
	{
   		return new efc::edb::EDatabase_mysql(proxy);
	}
}
