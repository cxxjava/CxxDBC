/*
 * EDatabase_pgsql.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "./EDatabase_pgsql.hh"

#include "./libpq/libpq-fs.h"
#include "./libpq/pg_type.h"

namespace efc {
namespace edb {

static void to_nbo(double in, double *out) {
    uint64_t *i = (uint64_t *)&in;
    uint32_t *r = (uint32_t *)out;

    /* convert input to network byte order */
    r[0] = htonl((uint32_t)((*i) >> 32));
    r[1] = htonl((uint32_t)*i);
}

//=============================================================================

EDatabase_pgsql::Result::~Result() {
	clear();
}

EDatabase_pgsql::Result::Result(PGresult* result, int fetchsize, int row_offset, int rows_size):
		fetch_size(fetchsize),
		row_offset(row_offset),
		rows_size(rows_size),
		current_row(0),
		pgResult(result),
		fields(NULL),
		rows(0),
		cols(0) {
	if (result && (PQresultStatus(result) == PGRES_TUPLES_OK)) {
		//fieldcount
		cols = PQnfields(result);
		//fields
		fields = new Field[cols];
		//rows
		rows = PQntuples(result);

		for (int i = 0; i < cols; i++) {
			char* name = PQfname(result, i);
			Oid type = PQftype(result, i);
			int length = PQfsize(result, i);
			int precision = PQfmod(result, i);
			//字段名,类型,长度,有效位数,比例|
			//FIXME! 无法得到比例值
			fields[i].name = name;
			fields[i].navite_type = type;
			fields[i].length = length;
			fields[i].precision = precision;
			fields[i].scale = -1;
			fields[i].type = CnvtNativeToStd(type, length, precision);
		}
	}
}

PGresult* EDatabase_pgsql::Result::getResult() {
	return pgResult;
}

void EDatabase_pgsql::Result::clear() {
	if (pgResult) {
		PQclear(pgResult);
		pgResult = NULL;
	}
	if (fields) {
		delete[] fields;
		fields = NULL;
	}
	current_row = rows = cols = 0;
}

int EDatabase_pgsql::Result::getRows() {
	return rows;
}

int EDatabase_pgsql::Result::getCols() {
	return cols;
}

EDatabase_pgsql::Result::Field* EDatabase_pgsql::Result::getField(int col) {
	if (col >= cols) {
		return NULL;
	}
	return &fields[col];
}

//=============================================================================

int EDatabase_pgsql::alterSQLPlaceHolder(const char *sql_com, EString& sql_pqsql) {
	boolean bQuotation = true;  //true-单引号为偶数个；false-单引号为奇数个。
	char *pPlaceHolder, *pQuotation;
	char *pPosPlaceHolder, *pPosQuotation;
	int placeholders = 0;

	pPosPlaceHolder = (char *)sql_com;
	pPosQuotation = (char *)sql_com;

	while (true) {
		pPlaceHolder = strchr(pPosPlaceHolder, SQL_PLACEHOLDER_CHAR);
		if (pPlaceHolder) {
			//判断'?'是不是有效的占位符，在单引号包含串内为非有效。
			while (1) {
				pQuotation = eso_strnchr(pPosQuotation, pPlaceHolder-pPosQuotation, SQL_STRING_QUOTATION);
				if (pQuotation) {
					bQuotation = !bQuotation;
					pPosQuotation = pQuotation + 1;

					continue;
				}
				else {
					pPosQuotation = pPlaceHolder + 1;

					break;
				}
			}

			if (!bQuotation) {  //单引号为奇数个
				sql_pqsql.append(pPosPlaceHolder, pPlaceHolder-pPosPlaceHolder+1);
			}
			else {
				sql_pqsql.append(pPosPlaceHolder, pPlaceHolder-pPosPlaceHolder);
				sql_pqsql.fmtcat("$%d", ++placeholders);
			}
			pPosPlaceHolder = pPlaceHolder + 1;

			continue;
		}
		else {
			sql_pqsql.append(pPosPlaceHolder);
			break;
		}
	}

	return placeholders;
}

void EDatabase_pgsql::createDataset(EBson *rep, int fetchsize, int offset,
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
	PGresult* result = m_Result->getResult();
	boolean eof = true;
	int count = 0;
	int total = m_Result->getRows();
	for (int i = m_Result->current_row; i < total; i++) {
		m_Result->current_row++;

		if (offset >= m_Result->current_row) //起始行
			continue;

		if (rows != 0 && (m_Result->current_row - offset) > rows) { //总记录数
			break;
		}

		//record
		EByteArrayOutputStream baos;
		EDataOutputStream dos(&baos);
		for (int j = 0; j < nFields; j++) {
			int len = PQgetlength(result, i, j);
			char *value = PQgetvalue(result, i, j);

			if (len <= 0 && PQgetisnull(result, i, j)) {
				dos.writeInt(-1); //is null
			} else {
				Result::Field *field = m_Result->getField(j);

				if (field->type == DB_dtBLob
					|| field->type == DB_dtLongBinary
					|| field->type == DB_dtBytes) {
					if (len > 0) {
						//@see: https://stackoverflow.com/questions/12196123/retrieving-file-from-bytea-in-postgresql-using-java
						size_t size = len;
						uchar *ptr = PQunescapeBytea((uchar*) value, &size);
						dos.writeInt(size);
						dos.write(ptr, size);
						PQfreemem(ptr);
					} else {
						dos.writeInt(0);
					}
				} else {
					dos.writeInt(len);
					dos.write(value, len);
				}
			}
		}
		rep->add(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD, baos.data(), baos.size());

		if (++count >= fetchsize) {
			eof = false;
			break;
		}
	}
	rep->setByte(EDB_KEY_DATASET "/" EDB_KEY_RECORDS, eof ? 1 : 0);  //更新EOF标记
}

EDatabase_pgsql::~EDatabase_pgsql() {
	close();
}

EDatabase_pgsql::EDatabase_pgsql(ELogger* workLogger, ELogger* sqlLogger,
		const char* clientIP, const char* version) :
		EDatabase(workLogger, sqlLogger, clientIP, version),
		m_Conn(null) {
}

EString EDatabase_pgsql::dbtype() {
	return EString("PGSQL");
}

EString EDatabase_pgsql::dbversion() {
	return EString(PQserverVersion(m_Conn));
}

boolean EDatabase_pgsql::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset, int timeout) {
	char connStrIn[256];

	snprintf(connStrIn, sizeof(connStrIn),
			 "host=%s "
			 "port=%d "
			 "dbname=%s "
			 "user=%s "
			 "password=%s "
			 "client_encoding=%s "
			 "connect_timeout=%d ", //seconds
			 host, port, database,
			 username, password, charset,
			 timeout);
	m_Conn = PQconnectdb(connStrIn);
	if (PQstatus(m_Conn) != CONNECTION_OK) {
		setErrorMessage(PQerrorMessage(m_Conn));
		return false;
	}

	if (PQsetClientEncoding(m_Conn, charset) != 0) {
		setErrorMessage(PQerrorMessage(m_Conn));
		PQfinish(m_Conn);
		return false;
	}

	/* SET DATESTYLE: 缺省设置ISO 8601-风格的日期和时间（YYYY-MM-DD HH:MM:SS）*/
	PGresult* result = PQexec(m_Conn, "set datestyle to ISO,YMD");
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		setErrorMessage(PQerrorMessage(m_Conn));
		PQclear(result);
		PQfinish(m_Conn);
		return false;
	}
	PQclear(result);

	return true;
}

boolean EDatabase_pgsql::EDatabase_pgsql::close() {
	m_Result.reset();

	//若启用了事务则默认进行回滚
	if (m_Conn) {
		PQfinish(m_Conn);
	}
	m_Conn = NULL;

	return true;
}

sp<EBson> EDatabase_pgsql::onExecute(EBson *req) {
	int fetchsize = req->getInt(EDB_KEY_FETCHSIZE, 4096);
	char *sql = req->get(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	es_bson_node_t *param_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
	PGresult* result;

	m_Result.reset();

	if (!param_node) {
		dumpSQL(sql);

		result = PQexec(m_Conn, sql);
	} else {
		EString pqsql;
		int param_count = req->count(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
		int param_count2 = alterSQLPlaceHolder(sql, pqsql);

		dumpSQL(sql, pqsql.c_str());

		if (param_count != param_count2) {
			setErrorMessage("params error");
			return null;
		}

		char *paramValues[param_count];
		int	paramLengths[param_count];
		int	paramFormats[param_count];

		int index = 0;
		while (param_node) {
			es_size_t size = 0;
			char *value = (char*)EBson::nodeGet(param_node, &size);
			ES_ASSERT(value && size);

			edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);
			int maxsize = EStream::readInt(value + 4);
			int datalen = EStream::readInt(value + 8);
			char *binddata = value + 12;

			if (stdtype == DB_dtReal) {
				ES_ASSERT(datalen == 8);
				llong l = eso_array2llong((es_byte_t*)binddata, datalen);
				double d = EDouble::llongBitsToDouble(l);
				//@see: https://stackoverflow.com/questions/42339752/inserting-a-floating-point-number-in-a-table-using-libpq
				double converted;
				to_nbo(d, &converted);
				memcpy(binddata, &converted, datalen);
			}

			paramValues[index] = (char *)binddata;
			paramLengths[index] = datalen;
			if (stdtype == DB_dtReal || stdtype == DB_dtBLob || stdtype == DB_dtBytes || stdtype == DB_dtLongBinary)
				paramFormats[index] = 1; //0-character 1-binary
			else
				paramFormats[index] = 0; //0-character 1-binary

			//next
			param_node = param_node->next;
			index++;
		}

		result = PQexecParams(m_Conn,
				pqsql.c_str(),
				param_count,         /* count of params */
				NULL,            /* let the backend deduce param type */
				paramValues,
				paramLengths,
				paramFormats,
				0);              /* ask for character results 0-character 1-binary*/
	}

	if (PQresultStatus(result) == PGRES_TUPLES_OK) {
		//result reset.
		m_Result = new Result(result, fetchsize, 0, 0);

		//resp pkg
		sp<EBson> rep = new EBson();
		createDataset(rep.get(), fetchsize, 0, 0, true);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

		//affected
		rep->add(EDB_KEY_AFFECTED, PQcmdTuples(result));

		return rep;
	} else {
		if (PQresultStatus(result) == PGRES_COMMAND_OK) {
			sp<EBson> rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
			//affected
			rep->add(EDB_KEY_AFFECTED, PQcmdTuples(result));
			PQclear(result);
			return rep;
		}
    }

	setErrorMessage(PQerrorMessage(m_Conn));
	PQclear(result);
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
static void addAffected(EBson* rep, int index, char* affected, const char* errmsg=NULL) {
	EString path(EDB_KEY_RESULT "/");
	path.append(index); // index
	if (!errmsg) {
		rep->add(path.c_str(), affected);
	} else {
		EByteBuffer bb;
		bb.appendFormat("%s?%s", affected, errmsg);
		rep->add(path.c_str(), bb.data(), bb.size());
	}
}
#define AFFECTED_SUCCESS(affected) do { addAffected(rep.get(), sqlIndex, affected, NULL); } while (0);
#define AFFECTED_FAILURE(count, errmsg) do { addAffected(rep.get(), sqlIndex, count, errmsg); hasFailed = true; } while (0);

sp<EBson> EDatabase_pgsql::onUpdate(EBson *req) {
	es_bson_node_t *sql_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	boolean isResume = req->getByte(EDB_KEY_RESUME, 0);
	boolean hasFailed = false;
	PGresult* result;
	int sqlIndex = 0;

	m_Result.reset();

	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

	while (sql_node) {
		if (!isResume && hasFailed) {
			break; //abort!
		}

		char *sql = EBson::nodeGet(sql_node);
		es_bson_node_t *params_node = EBson::childFind(sql_node, EDB_KEY_PARAMS);

		if (!params_node) {
			sqlIndex++;

			dumpSQL(sql);

			result = PQexec(m_Conn, sql);

			//affected
			if (PQresultStatus(result) == PGRES_COMMAND_OK) {
				AFFECTED_SUCCESS(PQcmdTuples(result));
			} else {
				AFFECTED_FAILURE(-1, PQerrorMessage(m_Conn));
			}
			PQclear(result);
		} else {
			EString pqsql;
			int param_count = alterSQLPlaceHolder(sql, pqsql);

			dumpSQL(sql, pqsql.c_str());

			int batch_count = EBson::childCount(sql_node, NULL);
			boolean prepared = false;

			if (batch_count > 1) {
				prepared = true;
				result = PQprepare(m_Conn,
						"", // stmtName
						pqsql.c_str(), // query
						param_count, // nParams
						NULL // paramTypes
						);
				if (PQresultStatus(result) != PGRES_COMMAND_OK) {
					sqlIndex++;
					AFFECTED_FAILURE(-batch_count, PQerrorMessage(m_Conn));
					sqlIndex += (batch_count - 1);
					sql_node = sql_node->next; //next
					PQclear(result);
					continue;
				}
				PQclear(result);
			}

			while (params_node) {
				sqlIndex++;

				if (!isResume && hasFailed) {
					break; //abort!
				}

				es_bson_node_t *param_node = EBson::childFind(params_node, EDB_KEY_PARAM);
				int param_count2 = EBson::childCount(params_node, EDB_KEY_PARAM);

				if (param_count != param_count2) {
					AFFECTED_FAILURE(-1, "bind params error.");
					params_node = params_node->next; //next
					continue;
				}

				Oid	paramTypes[param_count];
				char *paramValues[param_count];
				int	paramLengths[param_count];
				int	paramFormats[param_count];

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
						//@see: https://stackoverflow.com/questions/42339752/inserting-a-floating-point-number-in-a-table-using-libpq
						double converted;
						to_nbo(d, &converted);
						memcpy(binddata, &converted, datalen);
					}

					paramTypes[index] = CnvtStdToNative(stdtype);
					paramValues[index] = datalen ? (char *)binddata : NULL;
					paramLengths[index] = datalen;
					if (stdtype == DB_dtReal || stdtype == DB_dtBLob || stdtype == DB_dtBytes || stdtype == DB_dtLongBinary)
						paramFormats[index] = 1; //0-character 1-binary
					else
						paramFormats[index] = 0; //0-character 1-binary

					//next
					param_node = param_node->next;
					index++;
				}

				if (prepared) {
					result = PQexecPrepared(m_Conn,
							"", //stmtName
							param_count,         /* count of params */
							paramValues,
							paramLengths,
							paramFormats,
							0);              /* ask for character results 0-character 1-binary*/
				} else {
					result = PQexecParams(m_Conn,
							pqsql.c_str(),
							param_count,         /* count of params */
							NULL,            /* let the backend deduce param type */
							paramValues,
							paramLengths,
							paramFormats,
							0);              /* ask for character results 0-character 1-binary*/
				}

				//affected
				if (PQresultStatus(result) == PGRES_COMMAND_OK) {
					AFFECTED_SUCCESS(PQcmdTuples(result));
				} else {
					AFFECTED_FAILURE(-1,  PQerrorMessage(m_Conn));
				}
				PQclear(result);

				//next params
				params_node = params_node->next;
			}
		}

		//next sql.
		sql_node = sql_node->next;
	}

	return rep;
}

sp<EBson> EDatabase_pgsql::onMoreResult(EBson *req) {
	llong cursorID = req->getLLong(EDB_KEY_CURSOR);

	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}

	//resp pkg
	sp<EBson> rep = new EBson();
	//get multi result set ;( 目前postgresql好像只能返回单结果集
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

	//free old.
	m_Result.reset();

	return rep;
}

sp<EBson> EDatabase_pgsql::onResultFetch(EBson *req) {
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

sp<EBson> EDatabase_pgsql::onResultClose(EBson *req) {
	/*
	llong cursorID = req->getLLong(EDB_KEY_CURSOR);

	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}
	*/

	m_Result.reset();

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_pgsql::onCommit() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	PGresult *result = PQexec(m_Conn, "COMMIT");
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		setErrorMessage(PQerrorMessage(m_Conn));
		PQclear(result);
		return null;
	}
	PQclear(result);

	result = PQexec(m_Conn, "BEGIN");
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		m_AutoCommit = true;
		setErrorMessage(PQerrorMessage(m_Conn));
		PQclear(result);
		return null;
	}
	PQclear(result);

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_pgsql::onRollback() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	PGresult *result = PQexec(m_Conn, "ROLLBACK");
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		setErrorMessage(PQerrorMessage(m_Conn));
		PQclear(result);
		return null;
	}
	PQclear(result);

	result = PQexec(m_Conn, "BEGIN");
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		m_AutoCommit = true;
		setErrorMessage(PQerrorMessage(m_Conn));
		PQclear(result);
		return null;
	}
	PQclear(result);

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_pgsql::setAutoCommit(boolean autoCommit) {
	char *command = NULL;

	if (!autoCommit) { //关闭自动提交，即启动事务功能
		if (this->m_AutoCommit) {
			command = "BEGIN";
		}
	} else { //打开自动提交，即关闭事务功能
		if (!this->m_AutoCommit) {
			command = "COMMIT";
		}
	}

	if (command) {
		PGresult *result = PQexec(m_Conn, command);
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			setErrorMessage(PQerrorMessage(m_Conn));
			PQclear(result);
			return null;
		}
		PQclear(result);
	}
	this->m_AutoCommit = autoCommit;

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_pgsql::onLOBCreate() {
	sp<EBson> rep = new EBson();

	Oid	lobjId = lo_creat(m_Conn, INV_READ | INV_WRITE);
	if (lobjId == 0) {
		rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
	} else {
		rep->addLLong(EDB_KEY_OID, (llong)lobjId);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	}

	return rep;
}

sp<EBson> EDatabase_pgsql::onLOBWrite(llong oid, EInputStream *is) {
	sp<EBson> rep = new EBson();

	int lobj_fd = lo_open(m_Conn, (Oid)oid, INV_WRITE);
	if (lobj_fd < 0) {
		rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
	} else {
		char buf[8192];
		int nbytes, nwritten = 0;

		while ((nbytes = is->read(buf, sizeof(buf))) > 0) {
			//@see: https://www.postgresql.org/message-id/20070418140247.GA7356%40alvh.no-ip.org
			int n = lo_write(m_Conn, lobj_fd, buf, nbytes);
			if (n <= 0) {
				break;
			}
			nwritten += n;
		}

		rep->addLLong(EDB_KEY_WRITTEN, nwritten);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

		lo_close(m_Conn, lobj_fd);
	}

	return rep;
}

sp<EBson> EDatabase_pgsql::onLOBRead(llong oid, EOutputStream *os) {
	sp<EBson> rep = new EBson();

	int lobj_fd = lo_open(m_Conn, (Oid)oid, INV_READ);
	if (lobj_fd < 0) {
		rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
	} else {
		char buf[8192];
		int nbytes, nread = 0;

		while ((nbytes = lo_read(m_Conn, lobj_fd, buf,  sizeof(buf))) > 0) {
			if (nbytes <= 0) {
				break;
			}
			os->write(buf, nbytes);
			nread += nbytes;
		}

		rep->addLLong(EDB_KEY_READING, nread);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

		lo_close(m_Conn, lobj_fd);
	}

	return rep;
}

edb_field_type_e EDatabase_pgsql::CnvtNativeToStd(Oid type,
	                                      int length,
	                                      int precision)
{
	edb_field_type_e eDataType;

	switch(type)
	{
	case BOOLOID: // BOOLEAN|BOOL
		eDataType = DB_dtBool;
		break;

	case INT2OID: // INT2|SMALLINT - -32 thousand to 32 thousand, 2-byte storage
		eDataType = DB_dtShort;
		break;

	case INT4OID: // INT4|INTEGER - -2 billion to 2 billion integer, 4-byte storage
	case XIDOID: // transaction id
	case CIDOID: // command identifier type, sequence in transaction id
		eDataType = DB_dtInt;
		break;

	case INT8OID: // INT8|BIGINT - ~18 digit integer, 8-byte storage
		eDataType = DB_dtLong;
		break;

	case FLOAT4OID: // FLOAT4 - single-precision floating point number, 4-byte storage
	case FLOAT8OID: // FLOAT8 - double-precision floating point number, 8-byte storage
		eDataType = DB_dtReal;
		break;

	case CASHOID: // $d,ddd.cc, money
	case NUMERICOID: // NUMERIC(x,y) - numeric(precision, decimal), arbitrary precision number
		eDataType = DB_dtNumeric;
		break;

	case CHAROID: // CHAR - single character
	case BPCHAROID: // BPCHAR - char(length), blank-padded string, fixed storage length
	case NAMEOID: // NAME - 31-character type for storing system identifiers
	case VARCHAROID: // VARCHAR(?) - varchar(length), non-blank-padded string, variable storage length
		eDataType = DB_dtString;
		break;

	case BYTEAOID: // BYTEA - variable-length string, binary values escaped
		eDataType = DB_dtBytes;
		break;

	case TEXTOID: // TEXT - variable-length string, no limit specified
		eDataType = DB_dtLongChar;
		break;

	case OIDOID: // OID - object identifier(oid), maximum 4 billion
		eDataType = DB_dtBLob;
		break;

	case ABSTIMEOID: // ABSTIME - absolute, limited-range date and time (Unix system time)
	case DATEOID: // DATE - ANSI SQL date
	case TIMEOID: // TIME - hh:mm:ss, ANSI SQL time
	case TIMESTAMPOID: // TIMESTAMP - date and time
	case TIMESTAMPTZOID: // TIMESTAMP - date and time WITH TIMEZONE
	case TIMETZOID: // TIME WITH TIMEZONE - hh:mm:ss, ANSI SQL time
		eDataType = DB_dtDateTime;
		break;

	case UNKNOWNOID: // Unknown

	case INT2VECTOROID: // array of INDEX_MAX_KEYS int2 integers, used in system tables
	case REGPROCOID: // registered procedure
	case TIDOID: // (Block, offset), physical location of tuple
	case OIDVECTOROID: // array of INDEX_MAX_KEYS oids, used in system tables
	case POINTOID: // geometric point '(x, y)'
	case LSEGOID: // geometric line segment '(pt1,pt2)'
	case PATHOID: // geometric path '(pt1,...)'
	case BOXOID: // geometric box '(lower left,upper right)'
	case POLYGONOID: // geometric polygon '(pt1,...)'
	case LINEOID: // geometric line '(pt1,pt2)'

		// TODO: ???
	case RELTIMEOID: // relative, limited-range time interval (Unix delta time)
	case TINTERVALOID: // (abstime,abstime), time interval
	case INTERVALOID: // @ <number> <units>, time interval

	case CIRCLEOID: // geometric circle '(center,radius)'
	// locale specific
	case INETOID: // IP address/netmask, host address, netmask optional
	case CIDROID: // network IP address/netmask, network address
	case ZPBITOID: // BIT(?) - fixed-length bit string
	case VARBITOID: // BIT VARING(?) - variable-length bit string
		eDataType = DB_dtString;
		break;

	default:
		eDataType = DB_dtUnknown;
		break;
	}

	return eDataType;
}

Oid EDatabase_pgsql::CnvtStdToNative(edb_field_type_e eDataType)
{
	Oid dbtype;

	switch(eDataType)
	{
	case DB_dtBool:
		dbtype = BOOLOID;
		break;
	case DB_dtShort:
		dbtype = INT2OID;
		break;
	case DB_dtInt:
		dbtype = INT4OID;
		break;
	case DB_dtLong:
		dbtype = INT8OID;
		break;
	case DB_dtReal:
		dbtype = FLOAT8OID;
		break;
	case DB_dtNumeric:
		dbtype = NUMERICOID;
		break;
	case DB_dtDateTime:
		dbtype = TIMESTAMPOID;
		break;
	case DB_dtString:
		dbtype = VARCHAROID;
		break;
	case DB_dtBytes:
		dbtype = BYTEAOID;
		break;
	case DB_dtBLob:
		dbtype = OIDOID;
		break;
	case DB_dtCLob:
		dbtype = TEXTOID;
		break;
	default:
		dbtype = VARCHAROID;
		break;
	}

	return dbtype;
}

} /* namespace edb */
} /* namespace efc */

//=============================================================================

extern "C" {
	ES_DECLARE(efc::edb::EDatabase*) makeDatabase(ELogger* workLogger, ELogger* sqlLogger, const char* clientIP, const char* version)
	{
   		return new efc::edb::EDatabase_pgsql(workLogger, sqlLogger, clientIP, version);
	}
}
