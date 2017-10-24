/*
 * EDatabase_oracle.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "./EDatabase_oracle.hh"

#include <string.h>
#include <iostream>
#include <vector>

using namespace std;

namespace efc {
namespace edb {

static void write_stream_data(sp<EIterator<EInputStream*> > iter, Stream *im) {
	ES_ASSERT(iter != null);
	char buf[8192];
	int len;
	if (iter->hasNext()) {
		EInputStream* is = iter->next();
		while ((len = is->read(buf, sizeof(buf))) > 0) {
			im->writeBuffer(buf, len);
		}
		im->writeLastBuffer(NULL, 0);
	}
}

//=============================================================================

EDatabase_oracle::Result::~Result() {
	clear();
}

void EDatabase_oracle::Result::initFields(ResultSet *result) {
	if (result) {
		//fieldcount
		vector<MetaData> vMetaData = result->getColumnListMetaData();
		cols = (int)vMetaData.size();
		//fields
		fields = new Field[cols];
		//rows: Unknown, Oracle the database generally doesn't know how many rows a query will return until after it has fetched the last row.

		for (int i = 0; i < cols; i++) {
			int type = vMetaData[i].getInt(MetaData::ATTR_DATA_TYPE);
			int length = vMetaData[i].getInt(MetaData::ATTR_DATA_SIZE);
			int precision = vMetaData[i].getInt(MetaData::ATTR_PRECISION);
			int scale = vMetaData[i].getInt(MetaData::ATTR_SCALE);
			//字段名,类型,长度,有效位数,比例|
			fields[i].name = vMetaData[i].getString(MetaData::ATTR_NAME).c_str();
			fields[i].navite_type = type;
			fields[i].length = length;
			fields[i].precision = precision;
			fields[i].scale = scale;
			fields[i].type = CnvtNativeToStd(type, length, scale, precision);

			//bug? : ORA-32108: max column or parameter size not specified
			if (length == 0) {
				result->setMaxColumnSize(i+1, FIELD_MAX_SIZE);
			}
		}
	}
}

EDatabase_oracle::Result::Result(Statement* stmt, ResultSet* result,
		int fetchsize, int row_offset, int rows_size) :
		fetch_size(fetchsize),
		row_offset(row_offset),
		rows_size(rows_size),
		current_row(0),
		stmt(stmt),
		result(result),
		cursor_params_index(0),
		current_array_index(0),
		fields(NULL),
		cols(0) {
	initFields(result);
}

EDatabase_oracle::Result::Result(Statement* stmt, EA<int> cursors, int fetchsize, int offset, int rows) :
		fetch_size(fetchsize),
		row_offset(row_offset),
		rows_size(rows_size),
		current_row(0),
		stmt(stmt),
		result(null),
		cursor_params_index(cursors),
		current_array_index(0),
		fields(NULL),
		cols(0) {
	result = stmt->getCursor(cursor_params_index[current_array_index]);
	initFields(result);
}

ResultSet* EDatabase_oracle::Result::getResult() {
	return result;
}

void EDatabase_oracle::Result::clear() {
	if (result) {
		stmt->closeResultSet(result);
		result = NULL;
	}
	if (fields) {
		delete[] fields;
		fields = NULL;
	}
	current_row = cols = 0;
	cursor_params_index.reset(0);
	current_array_index = 0;
}

ResultSet* EDatabase_oracle::Result::next() {
	//clear old
	if (result) {
		stmt->closeResultSet(result);
		result = NULL;
	}
	if (fields) {
		delete[] fields;
		fields = NULL;
	}
	current_row = cols = 0;

	//next result
	if (cursor_params_index.length() == 0) {
		return null;
	} else {
		current_array_index++;
		if (current_array_index >= cursor_params_index.length()) {
			return null;
		} else {
			result = stmt->getCursor(cursor_params_index[current_array_index]);
			initFields(result);
			return result;
		}
	}
}

int EDatabase_oracle::Result::getCols() {
	return cols;
}

EDatabase_oracle::Result::Field* EDatabase_oracle::Result::getField(int col) {
	if (col >= cols) {
		return NULL;
	}
	return &fields[col];
}

//=============================================================================

int EDatabase_oracle::alterSQLPlaceHolder(const char *sql_com, EString& sql_ora) {
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
				sql_ora.append(pPosPlaceHolder, pPlaceHolder-pPosPlaceHolder+1);
			}
			else {
				sql_ora.append(pPosPlaceHolder, pPlaceHolder-pPosPlaceHolder);
				sql_ora.fmtcat(":%d", ++placeholders);
			}
			pPosPlaceHolder = pPlaceHolder + 1;

			continue;
		}
		else {
			sql_ora.append(pPosPlaceHolder);
			break;
		}
	}

	return placeholders;
}

void EDatabase_oracle::createDataset(EBson *rep, int fetchsize, int offset,
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
	ResultSet* result = m_Result->getResult();
	boolean eof = true;
	int count = 0;
	while (result->next()) {
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
			if (result->isNull(i+1)) {
				dos.writeInt(-1); //is null
			} else {
				Result::Field *field = m_Result->getField(i);

				if (field->type == DB_dtBLob
					|| field->type == DB_dtCLob
					|| field->type == DB_dtLongBinary
					|| field->type == DB_dtLongChar
					|| field->type == DB_dtBytes) {
					Stream *is = NULL;
					Clob cb;
					Blob bb;
					Bfile bf;

					switch (field->navite_type) {
					case SQLT_CLOB:
						cb = result->getClob(i+1);
						is = cb.getStream (1,0);
						break;
					case SQLT_BLOB:
						bb = result->getBlob(i+1);
						is = bb.getStream (1,0);
						break;
					case SQLT_BFILEE:
						bf = result->getBfile(i+1);
						is = bf.getStream (1,0);
						break;
					default:
						is = result->getStream(i+1);
						break;
					}

					if (is) {
						EByteBuffer bb(10240);
						char buf[8192];
						int bytesRead;

						while ((bytesRead = is->readBuffer(buf, sizeof(buf))) > 0) {
							bb.append(buf, bytesRead);
						}
						result->closeStream(is);

						dos.writeInt(bb.size());
						dos.write(bb.data(), bb.size());
					} else {
						dos.writeInt(0);
					}
				} else {
					string value = result->getString(i+1);
					if (field->navite_type == SQLT_AFC) {
						value.erase(value.find_last_not_of(" ") + 1);
					}
					dos.writeInt(value.length());
					dos.write(value.c_str(), value.length());
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

EDatabase_oracle::~EDatabase_oracle() {
	close();
}

EDatabase_oracle::EDatabase_oracle(EDBProxyInf* proxy) :
		EDatabase(proxy),
		m_Env(null), m_Conn(null), m_Stmt(null) {
}

EString EDatabase_oracle::dbtype() {
	return EString("ORACLE");
}

EString EDatabase_oracle::dbversion() {
	return EString(m_Conn->getServerVersion().c_str());
}

boolean EDatabase_oracle::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset, int timeout) {
	char connectString[256];

	//the connect string parameter format is 'host:port/database'.
	/*
	snprintf(connectString, sizeof(connectString),
	"(DESCRIPTION =(ADDRESS_LIST =(ADDRESS = (PROTOCOL = TCP)(HOST = %s)(PORT = %s)))"
	"(CONNECT_DATA = (SERVER = DEDICATED)(SERVICE_NAME = %s)))",
	ip, port, dbname);
	*/
	snprintf(connectString, sizeof(connectString), "%s:%d/%s", host, port, database);
	try {
		m_Env = Environment::createEnvironment((charset && *charset) ? charset : "utf8", "UTF8");
		m_Conn = m_Env->createConnection(username, password, connectString);
		m_Stmt = m_Conn->createStatement();
		m_Stmt->setAutoCommit(TRUE);

		m_Stmt->setSQL("ALTER SESSION SET NLS_TIMESTAMP_FORMAT = 'YYYY-MM-DD HH24:MI:SS'");
		m_Stmt->executeUpdate();
		m_Stmt->setSQL("ALTER SESSION SET NLS_DATE_FORMAT = 'YYYY-MM-DD HH24:MI:SS'");
		m_Stmt->executeUpdate();
	} catch (SQLException &e) {
		setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
		try {
			if (m_Stmt) m_Conn->terminateStatement(m_Stmt);
			if (m_Conn) m_Env->terminateConnection(m_Conn);
			if (m_Env) Environment::terminateEnvironment(m_Env);
		} catch (...) {
		}
		m_Stmt = null;
		m_Conn = null;
		m_Env = null;
		return false;
	}

	return true;
}

boolean EDatabase_oracle::EDatabase_oracle::close() {
	m_Result.reset();

	//若启用了事务则默认进行回滚
	try {
		if (!m_AutoCommit && m_Conn) m_Conn->rollback();

		if (m_Stmt) m_Conn->terminateStatement(m_Stmt);
		if (m_Conn) m_Env->terminateConnection(m_Conn);
		if (m_Env) Environment::terminateEnvironment(m_Env);
	} catch (SQLException &e) {
		setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
		return false;
	}

	m_Stmt = null;
	m_Conn = null;
	m_Env = null;

	return true;
}

sp<EBson> EDatabase_oracle::onExecute(EBson *req, EIterable<EInputStream*>* itb) {
	int fetchsize = req->getInt(EDB_KEY_FETCHSIZE, 4096);
	char *sql = req->get(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	es_bson_node_t *param_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
	ResultSet* result;
	Statement::Status stat;
	boolean hasCursor = false;
	EArrayList<int> cursorParamsIndex;

	m_Result.reset();

	try {
		if (!param_node) {
			dumpSQL(sql);

			//execute
			m_Stmt->setSQL(sql);
			stat = m_Stmt->execute();

		} else {
			EString orasql;
			int param_count = req->count(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
			int param_count2 = alterSQLPlaceHolder(sql, orasql);
			sp<EIterator<EInputStream*> > iter = itb ? itb->iterator() : null;

			dumpSQL(sql, orasql.c_str());

			if (param_count != param_count2) {
				setErrorMessage("params error");
				return null;
			}

			//set sql
			m_Stmt->setSQL(orasql.c_str());

			//hasCursor?
			es_bson_node_t *node = param_node;
			while (node) {
				es_size_t size = 0;
				char *value = (char*)EBson::nodeGet(node, &size);
				ES_ASSERT(value && size);

				edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);

				if (stdtype == DB_dtCursor) {
					hasCursor = true;
					break;
				}

				node = node->next;
			}

			//bind params
			boolean paramOutFlag[param_count];
			edb_field_type_e paramsTypes[param_count];
			char *paramValues[param_count];
			int	paramLengths[param_count];

			int index = 0;
			while (param_node) {
				es_size_t size = 0;
				char *value = (char*)EBson::nodeGet(param_node, &size);
				ES_ASSERT(value && size);

				edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);
				paramsTypes[index] = stdtype;
				int maxsize = 0;

				if (stdtype == DB_dtCursor) {
					paramOutFlag[index] = true;
					paramValues[index] = (char *)NULL;
					paramLengths[index] = 0;

					//register output parameters
					m_Stmt->registerOutParam(index+1, OCCICURSOR);
					cursorParamsIndex.add(index+1);
				} else { //!
					maxsize = EStream::readInt(value + 4);
					int datalen = EStream::readInt(value + 8);
					char *binddata = value + 12;

					if (stdtype == DB_dtReal && datalen == 8) {
						llong l = eso_array2llong((es_byte_t*)binddata, datalen);
						double d = EDouble::llongBitsToDouble(l);
						memcpy(binddata, &d, datalen);
					}

					paramOutFlag[index] = false;
					paramValues[index] = (char *)binddata;
					paramLengths[index] = datalen;
				}

				if (stdtype == DB_dtReal) {
					m_Stmt->setDouble(index+1, *(double*)paramValues[index]);
				} else {
					if (paramOutFlag[index] == false) {
						if (hasCursor) {
							//when call procedure, if set input parameters stream mode then resultset is empty!
							m_Stmt->setString(index+1, paramValues[index]);
							m_Stmt->setMaxParamSize(index+1, maxsize>0 ? maxsize : FIELD_MAX_SIZE);
						} else {
							if (stdtype == DB_dtReal || stdtype == DB_dtBLob || stdtype == DB_dtBytes || stdtype == DB_dtLongBinary) {
								m_Stmt->setBinaryStreamMode(index+1, maxsize>0 ? maxsize : FIELD_MAX_SIZE);
							} else {
								m_Stmt->setCharacterStreamMode(index+1, maxsize>0 ? maxsize : FIELD_MAX_SIZE);
							}
						}
					}
				}

				//next
				param_node = param_node->next;
				index++;
			}

			//execute
			m_Stmt->execute();

			//stream write
			if (!hasCursor) {
				for (int i=0; i<param_count; i++) {
					if (paramsTypes[i] == DB_dtReal) {
						continue;
					} else if (paramLengths[i] == -1) {
						Stream *im = m_Stmt->getStream(i+1);
						write_stream_data(iter, im);
						m_Stmt->closeStream(im);
					} else {
						Stream *im = m_Stmt->getStream(i+1);
						im->writeBuffer(paramValues[i], paramLengths[i]);
						im->writeLastBuffer(NULL, 0);
						m_Stmt->closeStream(im);
					}
				}
			}

			//status
			stat = m_Stmt->status();
		}

		if (hasCursor) {
			//result reset.
			m_Result = new Result(m_Stmt, cursorParamsIndex.toArray(), fetchsize, 0, 0);

			//resp pkg
			sp<EBson> rep = new EBson();
			createDataset(rep.get(), fetchsize, 0, 0, true);
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

			//affected
			rep->addLLong(EDB_KEY_AFFECTED, -1); //?

			return rep;

		} else if (stat == Statement::RESULT_SET_AVAILABLE) {
			result = m_Stmt->getResultSet();

			//result reset.
			m_Result = new Result(m_Stmt, result, fetchsize, 0, 0);

			//resp pkg
			sp<EBson> rep = new EBson();
			createDataset(rep.get(), fetchsize, 0, 0, true);
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);

			//affected
			rep->addLLong(EDB_KEY_AFFECTED, -1); //?

			return rep;
		} else if (stat == Statement::UPDATE_COUNT_AVAILABLE) {
			sp<EBson> rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
			//affected
			rep->addLLong(EDB_KEY_AFFECTED, m_Stmt->getUpdateCount());
			return rep;
		}
	} catch (SQLException &e) {
		setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
	}

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

sp<EBson> EDatabase_oracle::onUpdate(EBson *req, EIterable<EInputStream*>* itb) {
	es_bson_node_t *sql_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	boolean isResume = req->getByte(EDB_KEY_RESUME, 0);
	boolean hasFailed = false;
	ResultSet* result;
	int sqlIndex = 0;
	Statement::Status stat;

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

			try {
				m_Stmt->setSQL(sql);
				stat = m_Stmt->execute();

				//affected
				if (stat == Statement::UPDATE_COUNT_AVAILABLE)  {
					AFFECTED_SUCCESS(m_Stmt->getUpdateCount());
				} else {
					AFFECTED_FAILURE(-1, "Unknown error");
				}
			} catch (SQLException& e) {
				AFFECTED_FAILURE(-1, EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
			}

		} else {
			EString orasql;
			int param_count = alterSQLPlaceHolder(sql, orasql);
			sp<EIterator<EInputStream*> > iter = itb ? itb->iterator() : null;

			dumpSQL(sql, orasql.c_str());

			m_Stmt->setSQL(orasql.c_str());

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

				try {
					edb_field_type_e paramsTypes[param_count];
					char *paramValues[param_count];
					int	paramLengths[param_count];

					int index = 0;
					while (param_node) {
						es_size_t size = 0;
						char *value = (char*)EBson::nodeGet(param_node, &size);
						ES_ASSERT(value && size);

						edb_field_type_e stdtype = (edb_field_type_e)EStream::readInt(value);
						int maxsize = EStream::readInt(value + 4);
						int datalen = EStream::readInt(value + 8);
						char *binddata = value + 12;

						paramsTypes[index] = stdtype;
						paramValues[index] = (char *)binddata;
						paramLengths[index] = datalen;

						if (stdtype == DB_dtReal && datalen == 8) {
							llong l = eso_array2llong((es_byte_t*)binddata, datalen);
							double d = EDouble::llongBitsToDouble(l);
							m_Stmt->setDouble(index+1, d);
						} else if (stdtype == DB_dtBLob || stdtype == DB_dtBytes || stdtype == DB_dtLongBinary) {
							m_Stmt->setBinaryStreamMode(index+1, maxsize>0 ? maxsize : FIELD_MAX_SIZE);
						} else {
							m_Stmt->setCharacterStreamMode(index+1, maxsize>0 ? maxsize : FIELD_MAX_SIZE);
						}

						//next
						param_node = param_node->next;
						index++;
					}

					//execute
					m_Stmt->execute();

					//stream write
					for (int i=0; i<param_count; i++) {
						if (paramsTypes[i] == DB_dtReal) {
							continue;
						} else if (paramLengths[i] == -1) {
							Stream *im = m_Stmt->getStream(i+1);
							write_stream_data(iter, im);
							m_Stmt->closeStream(im);
						} else {
							Stream *im = m_Stmt->getStream(i+1);
							im->writeBuffer(paramValues[i], paramLengths[i]);
							im->writeLastBuffer(NULL, 0);
							m_Stmt->closeStream(im);
						}
					}

					//status
					stat = m_Stmt->status();

					//affected
					if (stat == Statement::UPDATE_COUNT_AVAILABLE)  {
						AFFECTED_SUCCESS(m_Stmt->getUpdateCount());
					} else {
						AFFECTED_FAILURE(-1, "Unknown error");
					}
				} catch (SQLException& e) {
					AFFECTED_FAILURE(-1, EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
				}

				//next params
				params_node = params_node->next;
			}
		}

		//next sql.
		sql_node = sql_node->next;
	}

	return rep;
}

/**
 * 注意：oracle仅支持游标型的多结果集返回
 */
sp<EBson> EDatabase_oracle::onMoreResult(EBson *req) {
	int fetchsize = (m_Result != null) ? m_Result->fetch_size : 8192;
	int affected = 0;

	llong cursorID = req->getLLong(EDB_KEY_CURSOR);
	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}

	sp<EBson> rep = new EBson();
	if (m_Result != null && m_Result->next()) {
		createDataset(rep.get(), fetchsize, 0, 0, true);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
		rep->addLLong(EDB_KEY_AFFECTED, affected);
	} else { // no more
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	}
	return rep;
}

sp<EBson> EDatabase_oracle::onResultFetch(EBson *req) {
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

sp<EBson> EDatabase_oracle::onResultClose(EBson *req) {
	//...

	m_Result.reset();

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_oracle::onCommit() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	try {
		m_Conn->commit();
	} catch (SQLException &e) {
		setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_oracle::onRollback() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	try {
		m_Conn->rollback();
	} catch (SQLException &e) {
		setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_oracle::setAutoCommit(boolean autoCommit) {
	if (this->m_AutoCommit != autoCommit) {
		try {
			m_Stmt->setAutoCommit(autoCommit);
		} catch (SQLException &e) {
			setErrorMessage(EString::formatOf("%d: %s", e.getErrorCode(), e.getMessage().c_str()).c_str());
			return null;
		}

		this->m_AutoCommit = autoCommit;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_oracle::onLOBCreate() {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_oracle::onLOBWrite(llong oid, EInputStream *is) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_oracle::onLOBRead(llong oid, EOutputStream *os) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

//@see: https://docs.oracle.com/cd/B19306_01/appdev.102/b14250/oci03typ.htm
edb_field_type_e EDatabase_oracle::CnvtNativeToStd(int type,
		unsigned int length, unsigned int scale, unsigned int precision)
{
	edb_field_type_e eDataType;

	switch(type)
	{
		case SQLT_AFC:
		case SQLT_CHR:
		case SQLT_RID: // ROWID	6 (10) bytes
		case SQLT_LAB: // MSLABEL	255 bytes
			eDataType = DB_dtString;
			break;
    	case SQLT_NUM:
    	case SQLT_VNU:
    		// if selecting constant or formula
			if (precision == 0)	// precision unknown
				eDataType = DB_dtNumeric;
			else if (scale > 0)
				eDataType = DB_dtNumeric;
			else	// check for exact type
			{
				int ShiftedPrec = precision - scale;	// not to deal with negative scale if it is
				if (ShiftedPrec < 5)	// -9,999 : 9,999
					eDataType = DB_dtShort;
				else if (ShiftedPrec < 10) // -999,999,999 : 999,999,999
					eDataType = DB_dtInt;
				else if (ShiftedPrec <= 15)
					eDataType = DB_dtLong;
				else
					eDataType = DB_dtNumeric;
			}
			break;
		case SQLT_INT:
			eDataType = DB_dtInt;
			break;
		case SQLT_UIN:
			eDataType = DB_dtLong;
			break;
		case SQLT_FLT:
		case SQLT_PDN:
			eDataType = DB_dtReal;
			break;
		case SQLT_DAT:
		case SQLT_DATE:
		case SQLT_TIME:
		case SQLT_TIME_TZ:
		case SQLT_TIMESTAMP:
		case SQLT_TIMESTAMP_TZ:
		case SQLT_INTERVAL_YM:
		case SQLT_INTERVAL_DS:
		case SQLT_TIMESTAMP_LTZ:
			eDataType = DB_dtDateTime;
			break;
		case SQLT_BIN:  //binary data(DTYBIN)
			eDataType = DB_dtBytes;
			break;
		case SQLT_LBI:  //long binary
		case SQLT_LVB:  //long long binary
			eDataType = DB_dtLongBinary;
			break;
		case SQLT_BLOB:
		case SQLT_BFILEE:
			eDataType = DB_dtBLob;
			break;
		case SQLT_LNG:  //long
		case SQLT_LVC:  //long longs char
			eDataType = DB_dtLongChar;
			break;
		case SQLT_CLOB:
		case SQLT_CFILEE:
			eDataType = DB_dtCLob;
			break;
		default:
			eDataType = DB_dtUnknown;
			break;
	}

	return eDataType;
}

//@see: https://docs.oracle.com/cd/B19306_01/appdev.102/b14250/oci03typ.htm
int EDatabase_oracle::CnvtStdToNative(edb_field_type_e eDataType)
{
	ub2 dty;

	switch(eDataType)
	{
	case DB_dtBool:
		// there is no "native" boolean type in Oracle,
		// so treat boolean as 16-bit signed INTEGER in Oracle
		dty = SQLT_INT;	// 16-bit signed integer
		break;
	case DB_dtShort:
		dty = SQLT_INT;	// 16-bit signed integer
		break;
	case DB_dtInt:
		dty = SQLT_INT;	// 32-bit signed integer
		break;
	case DB_dtReal:
		dty = SQLT_FLT;	// FLOAT
		break;
	case DB_dtLong:
	case DB_dtNumeric:
		dty = SQLT_NUM;	// NUMERIC
		break;
	case DB_dtDateTime:
		dty = SQLT_DAT;	// DATE
		break;
	case DB_dtString:
		// using SQLT_CHR can cause problems when
		// binding/comparing with CHAR(n) columns
		// like 'Select * from my_table where my_char_20 = :1'
		// if we used SQLT_CHR we would have to pad bind value with spaces
		//dty = SQLT_AFC;	// CHAR
		dty = SQLT_CHR;	// VARCHAR2
		break;
	case DB_dtBytes:
		dty = SQLT_BIN; // RAW
		break;
	case DB_dtLongBinary:
		dty = SQLT_LVB; // LONG VARRAW
		break;
	case DB_dtLongChar:
		dty = SQLT_LVC; // LONG VARCHAR
		break;
	case DB_dtBLob:
		dty = SQLT_BLOB;// Binary LOB
		break;
	case DB_dtCLob:
		dty = SQLT_CLOB;//Character LOB
		break;
	default:
		dty = SQLT_CHR;
		break;
	}

	return dty;
}

} /* namespace edb */
} /* namespace efc */

//=============================================================================

extern "C" {
	ES_DECLARE(efc::edb::EDatabase*) makeDatabase(efc::edb::EDBProxyInf* proxy)
	{
   		return new efc::edb::EDatabase_oracle(proxy);
	}
}
