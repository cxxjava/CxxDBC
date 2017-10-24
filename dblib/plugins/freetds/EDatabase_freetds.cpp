/*
 * EDatabase_freetds.hh
 *
 *  Created on: 2017-8-10
 *      Author: cxxjava@163.com
 */

#include "./EDatabase_freetds.hh"

//add for default date format
#include "./freetds/include/ctlib.h"
#include "./freetds/include/freetds/convert.h"

//CS_VERSION & CS_TDS_VERSION：
//@see: http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.infocenter.dc32840.1570/html/ctref/X49450.htm
//@see: http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.infocenter.dc32840.1570/html/ctref/X36927.htm
//数据类型：
//@see: https://doc.ispirer.cn/sqlways/Output/SQLWays-1-141.html
//@see: https://www.ibm.com/support/knowledgecenter/zh/SSZJPZ_9.1.0/com.ibm.swg.im.iis.conn.drs.doc/topics/DRS036.html
//@see: http://database.51cto.com/art/201011/235614.htm

#define CS_VERSION_SET      CS_VERSION_150

//#define DEBUG_SEND_DATA

#define PREPARE_STMT_NAME   "edb_dyn_stmt"

#define RETURN_NULL_IF(a,b) if (a != CS_SUCCEED) {setErrorMessageDiag(b); return null;}
#define RETURN_FALSE_IF(a,b) if (a != CS_SUCCEED) {setErrorMessageDiag(b); return false;}

namespace efc {
namespace edb {

static const char *
_cs_get_layer(int layer)
{
	switch (layer) {
	case 2:
		return "cslib user api layer";
		break;
	default:
		break;
	}
	return "unrecognized layer";
}

static const char *
_cs_get_origin(int origin)
{
	switch (origin) {
	case 1:
		return "external error";
		break;
	case 2:
		return "internal CS-Library error";
		break;
	case 4:
		return "common library error";
		break;
	case 5:
		return "intl library error";
		break;
	default:
		break;
	}
	return "unrecognized origin";
}

static const char *
_cs_get_user_api_layer_error(int error)
{
	switch (error) {
	case 3:
		return "Memory allocation failure.";
		break;
	case 16:
		return "Conversion between %1! and %2! datatypes is not supported.";
		break;
	case 20:
		return "The conversion/operation resulted in overflow.";
		break;
	case 24:
		return "The conversion/operation was stopped due to a syntax error in the source field.";
		break;
	default:
		break;
	}
	return "unrecognized error";
}

static char *
_cs_get_msgstr(const char *funcname, int layer, int origin, int severity, int number)
{
	char *m;

	if (asprintf(&m, "%s: %s: %s: %s", funcname, _cs_get_layer(layer), _cs_get_origin(origin), (layer == 2)
		     ? _cs_get_user_api_layer_error(number)
		     : "unrecognized error") < 0) {
		return NULL;
	}
	return m;
}

static void
_csclient_msg(CS_CONTEXT * ctx, const char *funcname, int layer, int origin, int severity, int number, const char *fmt, ...)
{
	va_list ap;
	CS_CLIENTMSG cm;
	char *msgstr;

	va_start(ap, fmt);

	if (ctx->_cslibmsg_cb) {
		cm.severity = severity;
		cm.msgnumber = (((layer << 24) & 0xFF000000)
				| ((origin << 16) & 0x00FF0000)
				| ((severity << 8) & 0x0000FF00)
				| ((number) & 0x000000FF));
		msgstr = _cs_get_msgstr(funcname, layer, origin, severity, number);
		tds_vstrbuild(cm.msgstring, CS_MAX_MSG, &(cm.msgstringlen), msgstr, CS_NULLTERM, fmt, CS_NULLTERM, ap);
		cm.msgstring[cm.msgstringlen] = '\0';
		free(msgstr);
		cm.osnumber = 0;
		cm.osstring[0] = '\0';
		cm.osstringlen = 0;
		cm.status = 0;
		/* cm.sqlstate */
		cm.sqlstatelen = 0;
		ctx->_cslibmsg_cb(ctx, &cm);
	}

	va_end(ap);
}

static CS_RETCODE
cs_convert2char(CS_CONTEXT * ctx, CS_DATAFMT * srcfmt, CS_VOID * srcdata,
		CS_DATAFMT * destfmt, es_data_t ** destdata, CS_INT * resultlen) {
	TDS_SERVER_TYPE src_type, desttype;
	int src_len, destlen, len, i = 0;
	CONV_RESULT cres;
	CS_RETCODE ret;

	src_type = _ct_get_server_type(NULL, srcfmt->datatype);
	if (src_type == TDS_INVALID_TYPE)
		return CS_FAIL;
	src_len = srcfmt->maxlength;
	desttype = _ct_get_server_type(NULL, destfmt->datatype);
	destlen = destfmt->maxlength;

	ES_ASSERT(src_type != desttype);
	ES_ASSERT(desttype == SYBCHAR);

	len = tds_convert(ctx->tds_ctx, src_type, (TDS_CHAR*) srcdata, src_len, desttype, &cres);

	switch (len) {
	case TDS_CONVERT_NOAVAIL:
		_csclient_msg(ctx, "cs_convert2char", 2, 1, 1, 16, "%d, %d", src_type, desttype);
		return CS_FAIL;
		break;
	case TDS_CONVERT_SYNTAX:
		_csclient_msg(ctx, "cs_convert2char", 2, 4, 1, 24, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_NOMEM:
		_csclient_msg(ctx, "cs_convert2char", 2, 4, 1, 3, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_OVERFLOW:
		_csclient_msg(ctx, "cs_convert2char", 2, 4, 1, 20, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_FAIL:
		return CS_FAIL;
		break;
	default:
		if (len < 0) {
			return CS_FAIL;
		}
		break;
	}

	ret = CS_SUCCEED;
	if (len > destlen) {
		len = destlen;
		ret = CS_FAIL;
	}
	switch (destfmt->format) {
	case CS_FMT_UNUSED:
		eso_mmemcpy(destdata, 0, cres.c, len);
		*resultlen = len;
		break;
	default:
		ret = CS_FAIL;
		break;
	}
	free(cres.c);

	return (ret);
}

static CS_RETCODE
handle_returns(CS_COMMAND *cmd, CS_INT *rowcount)
{
	CS_RETCODE results_ret;
	CS_INT result_type;

	/*
     ** Loop on a request to return result
     ** descriptions until no more are available
     */
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			if (rowcount) {
				//rows affected
				*rowcount = 0;
				ct_res_info(cmd, CS_ROW_COUNT, rowcount, CS_UNUSED, NULL);
			}
			break;
		case CS_CMD_FAIL:
			if (rowcount) {
				//rows affected
				*rowcount = 0;
				ct_res_info(cmd, CS_ROW_COUNT, rowcount, CS_UNUSED, NULL);
			}
			return CS_FAIL;
		default:
			return CS_FAIL;
		}
	}
	switch ((int) results_ret) {
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
		return CS_FAIL;
	default:
		return CS_FAIL;
	}

	return CS_SUCCEED ;
}

/* Run commands from which we expect no results returned */
static CS_RETCODE
run_command(CS_COMMAND *cmd, const char *sql, CS_INT *rowcount)
{
	CS_RETCODE ret;

	if (cmd == NULL) {
		return CS_FAIL;
	}

	ret = ct_command(cmd, CS_LANG_CMD, (CS_VOID *)sql, CS_NULLTERM, CS_UNUSED);
	if (ret != CS_SUCCEED) {
		return ret;
	}
	ret = ct_send(cmd);
	if (ret != CS_SUCCEED) {
		return ret;
	}

	return handle_returns(cmd, rowcount);
}

static CS_RETCODE create_prepare_stmt(CS_COMMAND* cmd, char* name, char* sql) {
	CS_RETCODE ret;
	CS_INT result_type;

	// try to cancel the old prepare.
	ct_dynamic(cmd, CS_DEALLOC, PREPARE_STMT_NAME, CS_NULLTERM, NULL, CS_UNUSED);
	ct_send(cmd);
	while(ct_results(cmd, &result_type) == CS_SUCCEED) ;

	ret = ct_dynamic(cmd, CS_PREPARE, name, CS_NULLTERM, sql, CS_NULLTERM);
	if (ret != CS_SUCCEED) return ret;
	ret = ct_send (cmd);
	if (ret != CS_SUCCEED) return ret;
	while(ct_results(cmd, &result_type) == CS_SUCCEED) ;

	return CS_SUCCEED;
}

static EByteBuffer* make_stream_data(sp<EIterator<EInputStream*> > iter) {
	ES_ASSERT(iter != null);
	char buf[8192];
	int len;
	if (iter->hasNext()) {
		EInputStream* is = iter->next();
		EByteBuffer* bb = new EByteBuffer();
		while ((len = is->read(buf, sizeof(buf))) > 0) {
			bb->append(buf, len);
		}
		return bb;
	}
	return null;
}

//=============================================================================

EDatabase_freetds::Result::~Result() {
	clear();
}

EDatabase_freetds::Result::Result(CS_COMMAND* cmd, int fetchsize, int row_offset, int rows_size):
		cmd(cmd),
		fetch_size(fetchsize),
		row_offset(row_offset),
		rows_size(rows_size),
		current_row(0),
		fields(null),
		cols(0) {
	ES_ASSERT(cmd);

	//fieldcount
	CS_RETCODE ret = ct_res_info(cmd, CS_NUMDATA, &cols, CS_UNUSED, NULL);
	//fields
	fields = new Field[cols];
	//rows: unknown

	for (int i = 0; i < cols; i++) {
		ct_describe(cmd, i+1, &fields[i].datafmt);
		CS_DATAFMT* descfmt = &fields[i].datafmt;

		CS_INT type = descfmt->datatype;
		CS_INT length = descfmt->maxlength;
		CS_INT scale = descfmt->scale;
		CS_INT precision = descfmt->precision;
		edb_field_type_e eDataType = CnvtNativeToStd(type, length, scale, precision);
		//字段名,类型,长度,有效位数,比例|
		fields[i].name = EString::formatOf("%*.*s", descfmt->namelen, descfmt->namelen, descfmt->name);
		fields[i].navite_type = type;
		fields[i].length = length;
		fields[i].precision = precision;
		fields[i].scale = scale;
		fields[i].type = eDataType;
	}
}

void EDatabase_freetds::Result::clear() {
	if (fields) {
		delete[] fields;
		fields = NULL;
	}
	current_row = cols = 0;
}

int EDatabase_freetds::Result::getCols() {
	return cols;
}

EDatabase_freetds::Result::Field* EDatabase_freetds::Result::getField(int col) {
	if (col >= cols) {
		return NULL;
	}
	return &fields[col];
}

//=============================================================================

int EDatabase_freetds::getRowsAffected() {
	int rows_affected = 0;
	ct_res_info(m_Cmd, CS_ROW_COUNT, &rows_affected, CS_UNUSED, NULL);
	return rows_affected;
}

void EDatabase_freetds::setErrorMessageDiag(const char *pre) {
	CS_INT num_msgs;
	CS_CLIENTMSG client_message;
	CS_SERVERMSG server_message;

	//pre
	if (pre) {
		m_ErrorMessage = pre;
		m_ErrorMessage.append("\n");
	} else {
		m_ErrorMessage.clear();
	}

	//total
	if (ct_diag(m_Conn, CS_STATUS, CS_ALLMSG_TYPE, CS_UNUSED, &num_msgs) != CS_SUCCEED) {
		return;
	}

	if (num_msgs <= 0) {
		return;
	}

	//client
	if (ct_diag(m_Conn, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &num_msgs) != CS_SUCCEED) {
		return;
	}

	if (num_msgs) {
		//只取最后一条错误消息
		if (ct_diag(m_Conn, CS_GET, CS_CLIENTMSG_TYPE, num_msgs, &client_message) != CS_SUCCEED) {
			return;
		}
		m_ErrorMessage.append("Open Client Message:\n");
		m_ErrorMessage.fmtcat("number %d layer %d origin %d severity %d number %d\n"
				"msgstring: %s\n"
				"osstring: %s\n",
				client_message.msgnumber,
				CS_LAYER(client_message.msgnumber),
				CS_ORIGIN(client_message.msgnumber),
				CS_SEVERITY(client_message.msgnumber),
				CS_NUMBER(client_message.msgnumber),
				client_message.msgstring,
				(client_message.osstringlen > 0) ? client_message.osstring : "(null)");
	}

	//server
	if (ct_diag(m_Conn, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &num_msgs) != CS_SUCCEED) {
		return;
	}

	if (num_msgs) {
		//只取最后一条错误消息
		if (ct_diag(m_Conn, CS_GET, CS_SERVERMSG_TYPE, num_msgs, &server_message) != CS_SUCCEED) {
			return;
		}
		m_ErrorMessage.append("Server Message:\n");
		m_ErrorMessage.fmtcat("number %d severity %d state %d line %d\n"
				"server: %s\n"
				"proc: %s\n"
				"text: %s\n",
				server_message.msgnumber,
				server_message.severity,
				server_message.state,
				server_message.line,
				(server_message.svrnlen > 0) ? server_message.svrname : "(null)",
				(server_message.proclen > 0) ? server_message.proc : "(null)",
				server_message.text);
	}

	//clear
	ct_diag(m_Conn, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, NULL);
}

boolean EDatabase_freetds::createDataset(EBson *rep, int fetchsize, int offset,
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
	CS_RETCODE ret;
	CS_INT result_type;
	CS_INT fetch_count;

	fetch_count = 0;
	while ((ret = ct_fetch(m_Cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &fetch_count)) == CS_SUCCEED) {
		if (fetch_count != 1) {
			return false;
		}
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
			Result::Field* field = m_Result->getField(i);
			EByteBuffer bb;
			char buf[4096];
			CS_INT bytesRead;

			while ((ret = ct_get_data(m_Cmd, i+1, buf, sizeof(buf),
					&bytesRead)) == CS_SUCCEED || ret == CS_END_ITEM
					|| ret == CS_END_DATA) {

				if (bytesRead > 0) {
					bb.append(buf, bytesRead);
				}

				if (ret == CS_END_ITEM || ret == CS_END_DATA)
					break;

				bytesRead = 0;
			}

			if (!(ret == CS_END_DATA || ret == CS_END_ITEM)) {
				return false;
			}

			int length = bb.size();
			void *value = bb.data();

			if (length == 0) {
				dos.writeInt(-1); //is null
			} else {
				// src_type == dst_type
				if (field->navite_type == CS_CHAR_TYPE) {
					dos.writeInt(length);
					dos.write(value, length);
				} else {
					// reset source data length to real size.
					field->datafmt.maxlength = bb.size();

					CS_DATAFMT dstfmt = {0};
					dstfmt.datatype = CS_CHAR_TYPE;
					dstfmt.count = 1;
					dstfmt.maxlength = FIELD_MAX_SIZE;
					CS_INT ret_len;
					es_data_t *dst_data = NULL;

					if (cs_convert2char(m_Ctx, &field->datafmt, bb.data(), &dstfmt, &dst_data,
							&ret_len) != CS_SUCCEED) {
						eso_mfree(dst_data);
						return false;
					}

					dos.writeInt(ret_len);
					dos.write(dst_data, ret_len);

					eso_mfree(dst_data);
				}
			}
		}
		rep->add(EDB_KEY_DATASET "/" EDB_KEY_RECORDS "/" EDB_KEY_RECORD, baos.data(), baos.size());

		if (++count >= fetchsize) {
			eof = false;
			break;
		}

		fetch_count = 0;
	}

	rep->setByte(EDB_KEY_DATASET "/" EDB_KEY_RECORDS, eof ? 1 : 0);  //更新EOF标记
	return true;
}

EDatabase_freetds::~EDatabase_freetds() {
	close();
}

EDatabase_freetds::EDatabase_freetds(EDBProxyInf* proxy) :
		EDatabase(proxy),
		m_Conn(null), m_Cmd(null) {
}

EString EDatabase_freetds::dbtype() {
	return  EString("FREETDS");
}

EString EDatabase_freetds::dbversion() {
	/*
	char string[1024];
	CS_INT len;
	CS_SMALLINT ind;
	CS_DATAFMT output[1];
	CS_RETCODE restype;
	ct_command(m_Cmd, CS_LANG_CMD, "select @@version", CS_NULLTERM, CS_UNUSED);
	ct_send(m_Cmd);
	ct_results(m_Cmd, &restype);
	ct_describe(m_Cmd, 1, &output[0]);
	ct_bind(m_Cmd, 1, &output[0], &string, &len, &ind);
	ct_fetch(m_Cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, NULL);
	return string;
	*/

	TDSCONNECTION *conn = &m_Conn->tds_socket->conn[0];
	int major = (conn->product_version >> 24) & 0x7F;
	int minor = (conn->product_version >> 16) & 0x7F;

	EString db(conn->product_name);
	db.append(" - ").append(major).append(".").append(minor);
	return db;
}

boolean EDatabase_freetds::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset, int timeout) {
	CS_RETCODE   ret;
	TDSCONTEXT  *tds_ctx;
	CS_LOCALE   *locale = NULL;

	ret = cs_ctx_alloc(CS_VERSION_SET, &m_Ctx);
	if (ret != CS_SUCCEED) {
		setErrorMessage("cs_ctx_alloc() failed!");
		return false;
	}

	tds_ctx = (TDSCONTEXT *)m_Ctx->tds_ctx;
	if (tds_ctx && tds_ctx->locale) {
		/* Force default date format, some tests rely on it */
		if (tds_ctx->locale->date_fmt) {
			free(tds_ctx->locale->date_fmt);
		}
		tds_ctx->locale->date_fmt = strdup("%Y-%m-%d %H:%M:%S");

		/* Force default server charset */
		if (charset && *charset) {
			if (tds_ctx->locale->server_charset) {
				free(tds_ctx->locale->server_charset);
			}
			tds_ctx->locale->server_charset = strdup(charset);
		}
	}

	ret = ct_init(m_Ctx, CS_VERSION_SET);
	if (ret != CS_SUCCEED) {
		setErrorMessage("ct_init() failed!");
		return false;
	}

	ret = ct_con_alloc(m_Ctx, &m_Conn);
	if (ret != CS_SUCCEED) {
		setErrorMessage("ct_con_alloc() failed!");
		return false;
	}

	ret = ct_diag(m_Conn, CS_INIT, CS_UNUSED, CS_UNUSED, NULL);
	if (ret != CS_SUCCEED) {
		setErrorMessage("cs_diag(CS_INIT) failed!");
		return false;
	}

	if (ct_con_props(m_Conn, CS_SET, CS_USERNAME, (CS_VOID*)username, CS_NULLTERM, NULL) != CS_SUCCEED
		|| ct_con_props(m_Conn, CS_SET, CS_PASSWORD, (CS_VOID*)password, CS_NULLTERM, NULL) != CS_SUCCEED
		|| ct_con_props(m_Conn, CS_SET, CS_HOSTNAME, (CS_VOID*)host, CS_NULLTERM, NULL) != CS_SUCCEED
		|| ct_con_props(m_Conn, CS_SET, CS_PORT, (CS_VOID*)&port, CS_UNUSED, NULL) != CS_SUCCEED
		|| ct_con_props(m_Conn, CS_SET, CS_TIMEOUT, (CS_VOID*)&timeout, CS_UNUSED, NULL) != CS_SUCCEED
		) {
		setErrorMessageDiag();
		return false;
	}

	/*
	 * The following section initializes all locale type information.
	 * First, we need to create a locale structure and initialize it
	 * with information.
	 */
	cs_loc_alloc(m_Ctx, &locale );

	/*-- Initialize --*/
	cs_locale(m_Ctx, CS_SET, locale, CS_LC_ALL, (CS_CHAR*)NULL, CS_UNUSED, (CS_INT*)NULL);

	/*-- Character Set --*/
	if (charset && *charset) {
		cs_locale(m_Ctx, CS_SET, locale, CS_SYB_CHARSET, (CS_CHAR*)charset, CS_NULLTERM, (CS_INT*)NULL);
		tds_set_client_charset(m_Conn->tds_login, charset);
	}

	/*-- Locale Property --*/
	ct_con_props(m_Conn, CS_SET, CS_LOC_PROP, (CS_VOID*)locale, CS_UNUSED, (CS_INT*)NULL);

	cs_loc_drop(m_Ctx, locale);
	/* end set locale */

	/*-- Timeout Set --*/
	if (timeout > 0) {
		m_Conn->tds_login->connect_timeout = timeout;
		m_Conn->tds_login->query_timeout = timeout;
	}

	// do connect
	ret = ct_connect(m_Conn, (CS_CHAR*)host, CS_NULLTERM);
	RETURN_FALSE_IF(ret, "");

	ret = ct_cmd_alloc(m_Conn, &m_Cmd);
	RETURN_FALSE_IF(ret, "");

	// change database
	EString query = EString::formatOf("use %s", database);
	ret = run_command(m_Cmd, query.c_str(), NULL);
	RETURN_FALSE_IF(ret, "");

	return true;
}

boolean EDatabase_freetds::EDatabase_freetds::close() {
	//what it to do?
	m_Result.reset();

	if (m_Conn) {
		ct_cancel(m_Conn, NULL, CS_CANCEL_ALL);
	}

	if (m_Cmd) {
		ct_cmd_drop(m_Cmd);
	}
	if (m_Conn) {
		ct_close(m_Conn, CS_UNUSED);
		ct_con_drop(m_Conn);
	}
	if (m_Ctx) {
		ct_exit(m_Ctx, CS_UNUSED);
		cs_ctx_drop(m_Ctx);
	}

	m_Cmd = NULL;
	m_Conn = NULL;
	m_Ctx = NULL;

	return true;
}

sp<EBson> EDatabase_freetds::onExecute(EBson *req, EIterable<EInputStream*>* itb) {
	int fetchsize = req->getInt(EDB_KEY_FETCHSIZE, 4096);
	char *sql = req->get(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	es_bson_node_t *param_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);

	CS_RETCODE ret;
	CS_INT result_type;

	while(ct_results(m_Cmd, &result_type) == CS_SUCCEED) ;
	m_Result.reset();

	dumpSQL(sql);

	if (!param_node) {

		ret = ct_command(m_Cmd, CS_LANG_CMD, sql, CS_NULLTERM, CS_UNUSED);
		RETURN_NULL_IF(ret, "");

		ret = ct_send(m_Cmd);
		RETURN_NULL_IF(ret, "");

	} else {

		sp<EIterator<EInputStream*> > iter = itb ? itb->iterator() : null;
		int param_count = req->count(EDB_KEY_SQLS "/" EDB_KEY_SQL "/" EDB_KEY_PARAMS "/" EDB_KEY_PARAM);
		int param_count2 = tds_count_placeholders(sql);

		if (param_count != param_count2) {
			setErrorMessage("params error");
			return null;
		}

		/**
		 * Send the prepared statement to the server
		 */
		ret = create_prepare_stmt(m_Cmd, PREPARE_STMT_NAME, sql);
		RETURN_NULL_IF(ret, "");

		char *paramValues[param_count];
		int	paramLengths[param_count];
		CS_DATAFMT paramFormats[param_count];
		memset(paramFormats, 0, sizeof(paramFormats));
		EA<EByteBuffer*> paramStreams(param_count);

#if 0
		ret = ct_dynamic(m_Cmd, CS_DESCRIBE_INPUT, PREPARE_STMT_NAME, CS_NULLTERM, NULL, CS_UNUSED);
		RETURN_NULL_IF(ret, "");
		ret = ct_send (m_Cmd);
		RETURN_NULL_IF(ret, "");

		while (ct_results(m_Cmd, &result_type) == CS_SUCCEED) {
			if (result_type == CS_DESCRIBE_RESULT) {
				CS_INT num_param, outlen;
				int i;
				ret = ct_res_info(m_Cmd, CS_NUMDATA, &num_param, CS_UNUSED, &outlen);
				RETURN_NULL_IF(ret, "");
				for (i = 1; i <= num_param; ++i) {
					ct_describe(m_Cmd, i, &paramFormats[i - 1]);
					printf("column type: %d size: %d\n",
							paramFormats[i - 1].datatype, paramFormats[i - 1].maxlength);
				}
			}
		}
#endif

		/**
		 * Execute the dynamic statement.
		 */
		ret = ct_dynamic(m_Cmd, CS_EXECUTE, PREPARE_STMT_NAME, CS_NULLTERM, NULL, CS_UNUSED);
		RETURN_NULL_IF(ret, "");

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

			// stream param, and libpq unsupported send data mode.
			if (datalen == -1) {
				paramStreams[index] = make_stream_data(iter);
				binddata = (char*)paramStreams[index]->data();
				datalen = paramStreams[index]->size();
			}

			paramValues[index] = (char *)binddata;
			paramLengths[index] = datalen;

			paramFormats[index].datatype = CnvtStdToNative(stdtype);
			paramFormats[index].count = 1;
			paramFormats[index].namelen = CS_NULLTERM;
			paramFormats[index].status = CS_INPUTVALUE;

			switch (paramFormats[index].datatype) {
			case CS_IMAGE_TYPE:
			case CS_TEXT_TYPE:
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
						paramLengths[index], 0);
				break;
			case CS_BIT_TYPE: {
				EString s((char*)paramValues[index], 0, paramLengths[index]);
				byte v = EBoolean::parseBoolean(s.c_str()) ? 1 : 0;
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
						sizeof(byte), 0);
				break;
			}
			case CS_SMALLINT_TYPE: {
				EString s((char*)paramValues[index], 0, paramLengths[index]);
				short v = EShort::parseShort(s.c_str());
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
						sizeof(short), 0);
				break;
			}
			case CS_INT_TYPE: {
				EString s((char*)paramValues[index], 0, paramLengths[index]);
				int v = EInteger::parseInt(s.c_str());
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
						sizeof(int), 0);
				break;
			}
			case CS_BIGINT_TYPE: {
				EString s((char*)paramValues[index], 0, paramLengths[index]);
				llong v = ELLong::parseLLong(s.c_str());
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
						sizeof(llong), 0);
				break;
			}
			case CS_FLOAT_TYPE: {
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
						8, 0);
				break;
			}
			case CS_NUMERIC_TYPE: {
				CS_DATAFMT srcfmt = {0};
				srcfmt.datatype = CS_CHAR_TYPE;
				srcfmt.maxlength = paramLengths[index];

				// set scale
				EString s(paramValues[index], 0, paramLengths[index]);
				EBigDecimal bd(s.c_str());
				paramFormats[index].precision = 37; //max precision!
				paramFormats[index].scale = bd.scale();

				CS_NUMERIC n = {0};
				ret = cs_convert(m_Ctx, &srcfmt, (CS_VOID *)paramValues[index],
						&paramFormats[index], (CS_VOID *)&n, 0);

				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&n,
						sizeof(CS_NUMERIC), 0);
				break;
			}
#if 0
			case CS_DATETIME_TYPE: {

				break;
			}
			case CS_BINARY_TYPE: {

				break;
			}
#endif
			default:
				ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
						paramLengths[index], 0);
				break;
			}
			RETURN_NULL_IF(ret, "");

			//next
			param_node = param_node->next;
			index++;
		}

		/**
		 * Insure that all the data is sent to the server.
		 */
		ret = ct_send(m_Cmd);
		RETURN_NULL_IF(ret, "");
	}

	ret = ct_results(m_Cmd, &result_type);
	RETURN_NULL_IF(ret, "");

	if (result_type == CS_ROW_RESULT) {
		//result reset.
		m_Result = new Result(m_Cmd, fetchsize, 0, 0);

		//resp pkg
		sp<EBson> rep = new EBson();
		createDataset(rep.get(), fetchsize, 0, 0, true);
		rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
		//affected
		rep->addLLong(EDB_KEY_AFFECTED, getRowsAffected());
		return rep;
	} else {
		switch ((int)result_type) {
			case CS_CMD_SUCCEED:
			case CS_CMD_DONE: {
				sp<EBson> rep = new EBson();
				rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
				//affected
				rep->addLLong(EDB_KEY_AFFECTED, getRowsAffected());
				return rep;
			}
			default: {
				break;
			}
		}
	}

	setErrorMessageDiag();
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

sp<EBson> EDatabase_freetds::onUpdate(EBson *req, EIterable<EInputStream*>* itb) {
	es_bson_node_t *sql_node = req->find(EDB_KEY_SQLS "/" EDB_KEY_SQL);
	boolean isResume = req->getByte(EDB_KEY_RESUME, 0);
	boolean hasFailed = false;
	int sqlIndex = 0;
	CS_RETCODE ret;
	CS_INT result_type;

	while(ct_results(m_Cmd, &result_type) == CS_SUCCEED) ;
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
			CS_INT rowcount;
			if (run_command(m_Cmd, sql, &rowcount) == CS_SUCCEED) {
				AFFECTED_SUCCESS(rowcount);
			} else {
				setErrorMessageDiag();
				AFFECTED_FAILURE(-1, getErrorMessage().c_str());
			}

		} else { //!

			sp<EIterator<EInputStream*> > iter = itb ? itb->iterator() : null;
			int param_count = tds_count_placeholders(sql);
			int batch_count = EBson::childCount(sql_node, NULL);
			boolean prepared = false;

			/** FIXME: blob字段写必须prepare模式？？？
			  Server Message:
			  number 3811 severity 18 state 4 line -1
			  server: DKSYBASE
			  proc: (null)
			  text: A wrong datastream has been sent to the server. The server was expecting token 0 but got the token 16. This is an internal error.
			 */
			//@see: http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.help.sdk_12.5.1.ctref/html/ctref/CHDEHFBA.htm
			batch_count = 2; //!

			if (batch_count > 1) {
				prepared = true;

				/**
				 * Send the prepared statement to the server
				 */
				ret = create_prepare_stmt(m_Cmd, PREPARE_STMT_NAME, sql);
			} else {
				ret = ct_command(m_Cmd, CS_LANG_CMD, sql, CS_NULLTERM, CS_UNUSED);
			}

			if (ret != CS_SUCCEED) {
				setErrorMessageDiag();
				sqlIndex++;
				AFFECTED_FAILURE(-batch_count, getErrorMessage().c_str());
				sqlIndex += (batch_count - 1);
				sql_node = sql_node->next; //next
				continue;
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

				if (prepared) {
					/**
					 * Execute the dynamic statement.
					 */
					ret = ct_dynamic(m_Cmd, CS_EXECUTE, PREPARE_STMT_NAME, CS_NULLTERM, NULL, CS_UNUSED);
					if (ret != CS_SUCCEED) {
						AFFECTED_FAILURE(-1, "ct_dynamic execute failed.");
						params_node = params_node->next; //next
						continue;
					}
				}

				char *paramValues[param_count];
				int	paramLengths[param_count];
				CS_DATAFMT paramFormats[param_count];
				memset(paramFormats, 0, sizeof(paramFormats));
				EA<EByteBuffer*> paramStreams(param_count);

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

					// stream param, and libpq unsupported send data mode.
					if (datalen == -1) {
						paramStreams[index] = make_stream_data(iter);
						binddata = (char*)paramStreams[index]->data();
						datalen = paramStreams[index]->size();
					}

					paramValues[index] = (char *)binddata;
					paramLengths[index] = datalen;

					paramFormats[index].datatype = CnvtStdToNative(stdtype);
					paramFormats[index].count = 1;
					paramFormats[index].namelen = CS_NULLTERM;
					paramFormats[index].status = CS_INPUTVALUE;

					switch (paramFormats[index].datatype) {
					case CS_IMAGE_TYPE:
					case CS_TEXT_TYPE:
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
								paramLengths[index], 0);
						break;
					case CS_BIT_TYPE: {
						EString s((char*)paramValues[index], 0, paramLengths[index]);
						CS_BIT v = EBoolean::parseBoolean(s.c_str()) ? 1 : 0;
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
								sizeof(byte), 0);
						break;
					}
					case CS_SMALLINT_TYPE: {
						EString s((char*)paramValues[index], 0, paramLengths[index]);
						short v = EShort::parseShort(s.c_str());
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
								sizeof(short), 0);
						break;
					}
					case CS_INT_TYPE: {
						EString s((char*)paramValues[index], 0, paramLengths[index]);
						int v = EInteger::parseInt(s.c_str());
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
								sizeof(int), 0);
						break;
					}
					case CS_BIGINT_TYPE: {
						EString s((char*)paramValues[index], 0, paramLengths[index]);
						llong v = ELLong::parseLLong(s.c_str());
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&v,
								sizeof(llong), 0);
						break;
					}
					case CS_FLOAT_TYPE: {
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
								8, 0);
						break;
					}
					case CS_NUMERIC_TYPE: {
						CS_DATAFMT srcfmt = {0};
						srcfmt.datatype = CS_CHAR_TYPE;
						srcfmt.maxlength = paramLengths[index];

						// set scale
						EString s(paramValues[index], 0, paramLengths[index]);
						EBigDecimal bd(s.c_str());
						paramFormats[index].precision = 37; //max precision!
						paramFormats[index].scale = bd.scale();

						CS_NUMERIC n = {0};
						ret = cs_convert(m_Ctx, &srcfmt, (CS_VOID *)paramValues[index],
								&paramFormats[index], (CS_VOID *)&n, 0);

						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)&n,
								sizeof(CS_NUMERIC), 0);
						break;
					}
					default:
						ret = ct_param(m_Cmd, &paramFormats[index], (CS_VOID *)paramValues[index],
								paramLengths[index], 0);
						break;
					}

					if (ret != CS_SUCCEED) {
						//...
						break;
					}

					//next
					param_node = param_node->next;
					index++;
				}

				/**
				 * Insure that all the data is sent to the server.
				 */
				ret = ct_send(m_Cmd);

				//affected
				ret = ct_results(m_Cmd, &result_type);
				if (ret == CS_SUCCEED) {
					switch ((int)result_type) {
						case CS_CMD_SUCCEED:
						case CS_CMD_DONE: {
							AFFECTED_SUCCESS(getRowsAffected());
							break;
						}
						default: {
							setErrorMessageDiag();
							AFFECTED_FAILURE(-1, getErrorMessage().c_str());
							break;
						}
					}
				} else {
					setErrorMessageDiag();
					AFFECTED_FAILURE(-1,  getErrorMessage().c_str());
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

sp<EBson> EDatabase_freetds::onMoreResult(EBson *req) {
	int fetchsize = (m_Result != null) ? m_Result->fetch_size : 8192;

	llong cursorID = req->getLLong(EDB_KEY_CURSOR);
	if (cursorID != currCursorID() || !m_Result) {
		setErrorMessage("cursor error.");
		return null;
	}

	CS_RETCODE ret;
	CS_INT res_type;

	while ((ret = ct_results(m_Cmd, &res_type)) == CS_SUCCEED) {
		switch (res_type) {
		case CS_ROW_RESULT: {
			//result reset.
			m_Result = new Result(m_Cmd, fetchsize, 0, 0);

			sp<EBson> rep = new EBson();
			createDataset(rep.get(), fetchsize, 0, 0, true);
			rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
			rep->addLLong(EDB_KEY_AFFECTED, getRowsAffected());
			return rep;
			break;
		}
		case CS_CMD_FAIL:
			setErrorMessageDiag();
			return null;
			break;
		default:
			break;
		}
	}

	// no more
	sp<EBson> rep = new EBson();
	rep->addInt(EDB_KEY_ERRCODE, ES_SUCCESS);
	return rep;
}

sp<EBson> EDatabase_freetds::onResultFetch(EBson *req) {
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

sp<EBson> EDatabase_freetds::onResultClose(EBson *req) {
	CS_INT result_type;

	while(ct_results(m_Cmd, &result_type) == CS_SUCCEED) ;
	m_Result.reset();

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::onCommit() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	if (run_command(m_Cmd, "commit tran", NULL) != CS_SUCCEED) {
		setErrorMessageDiag("commit tran failed!");
		return null;
	}

	if (run_command(m_Cmd, "begin tran", NULL) != CS_SUCCEED) {
		m_AutoCommit = true;
		setErrorMessageDiag("begin tran failed!");
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::onRollback() {
	if (m_AutoCommit) {
		setErrorMessage("auto commit mode.");
		return null;
	}

	if (run_command(m_Cmd, "rollback tran", NULL) != CS_SUCCEED) {
		setErrorMessageDiag("rollback tran failed!");
		return null;
	}

	if (run_command(m_Cmd, "begin tran", NULL) != CS_SUCCEED) {
		m_AutoCommit = TRUE;
		setErrorMessageDiag("begin tran failed!");
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::setAutoCommit(boolean autoCommit) {
	if (this->m_AutoCommit != autoCommit) {
		if (autoCommit) {
			if (run_command(m_Cmd, "commit tran", NULL) != CS_SUCCEED) {
				setErrorMessageDiag("commit tran failed!");
				return null;
			}
		} else {
			if (run_command(m_Cmd, "begin tran", NULL) != CS_SUCCEED) {
				setErrorMessageDiag("begin tran failed!");
				return null;
			}
		}

		this->m_AutoCommit = autoCommit;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::setSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	if (run_command(m_Cmd, EString::formatOf("save tran %s", name.c_str()).c_str(), NULL) != CS_SUCCEED) {
		setErrorMessageDiag("save tran failed!");
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::rollbackSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	if (run_command(m_Cmd, EString::formatOf("rollback tran %s", name.c_str()).c_str(), NULL) != CS_SUCCEED) {
		setErrorMessageDiag("rollback tran failed!");
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::releaseSavepoint(EBson *req) {
	EString name = req->getString(EDB_KEY_NAME);
	if (run_command(m_Cmd, EString::formatOf("commit tran %s", name.c_str()).c_str(), NULL) != CS_SUCCEED) {
		setErrorMessageDiag("commit tran failed!");
		return null;
	}

	//resp pkg
	return genRspCommSuccess();
}

sp<EBson> EDatabase_freetds::onLOBCreate() {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_freetds::onLOBWrite(llong oid, EInputStream *is) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

sp<EBson> EDatabase_freetds::onLOBRead(llong oid, EOutputStream *os) {
	setErrorMessage("unsupported");
	return genRspCommFailure();
}

edb_field_type_e EDatabase_freetds::CnvtNativeToStd(CS_INT type,
		CS_INT length, CS_INT scale, CS_INT precision) {
	edb_field_type_e eDataType;

	switch (type) {
		case CS_CHAR_TYPE:
		case CS_LONGCHAR_TYPE:
			eDataType = DB_dtString;
			break;
		case CS_BINARY_TYPE:
		case CS_LONGBINARY_TYPE:
			eDataType = DB_dtBytes;
			break;
		case CS_TEXT_TYPE:
			eDataType = DB_dtCLob;
			break;
		case CS_IMAGE_TYPE:
			eDataType = DB_dtBLob;
			break;
		case CS_TINYINT_TYPE:
		case CS_SMALLINT_TYPE:
			eDataType = DB_dtShort;
			break;
		case CS_INT_TYPE:
			eDataType = DB_dtInt;
			break;
		case CS_BIGINT_TYPE:
			eDataType = DB_dtLong;
			break;
		case CS_REAL_TYPE:
		case CS_FLOAT_TYPE:
			eDataType = DB_dtReal;
			break;
		case CS_BIT_TYPE:
			eDataType = DB_dtBool;
			break;
		case CS_DATETIME_TYPE:
		case CS_DATETIME4_TYPE:
			eDataType = DB_dtDateTime;
			break;
		case CS_MONEY_TYPE:
		case CS_MONEY4_TYPE:
			eDataType = DB_dtNumeric;
			break;
		case CS_NUMERIC_TYPE:
		case CS_DECIMAL_TYPE:
			if (scale > 0)
				eDataType = DB_dtNumeric;
			else	// check for exact type
			{
				if (precision < 5)
					eDataType = DB_dtShort;
				else if(precision < 10)
					eDataType = DB_dtLong;
				else
					eDataType = DB_dtNumeric;
			}
			break;
		case CS_VARCHAR_TYPE:
			eDataType = DB_dtString;
			break;
		case CS_VARBINARY_TYPE:
			eDataType = DB_dtBytes;
			break;
		case CS_LONG_TYPE:
			eDataType = DB_dtLong;
			break;
		case CS_SENSITIVITY_TYPE:
		case CS_BOUNDARY_TYPE:
		case CS_VOID_TYPE:
			eDataType = DB_dtUnknown;
			break;
		case CS_USHORT_TYPE:
			eDataType = DB_dtInt;
			break;
		case CS_UNICHAR_TYPE:
			eDataType = DB_dtString;
			break;
		case CS_BLOB_TYPE:
			eDataType = DB_dtBLob;
			break;
		case CS_DATE_TYPE:
		case CS_TIME_TYPE:
			eDataType = DB_dtDateTime;
			break;
		case CS_UNITEXT_TYPE:
			eDataType = DB_dtCLob;
			break;
		case CS_USMALLINT_TYPE:
			eDataType = DB_dtShort;
			break;
		case CS_UINT_TYPE:
			eDataType = DB_dtLong;
			break;
		case CS_UBIGINT_TYPE:
			eDataType = DB_dtNumeric;
			break;
		case CS_XML_TYPE:  //?
			eDataType = DB_dtCLob;
			break;
		default:
			eDataType = DB_dtUnknown;
			break;
	}

	return eDataType;
}

CS_INT EDatabase_freetds::CnvtStdToNative(edb_field_type_e eDataType) {
	CS_INT type;

	switch (eDataType) {
	case DB_dtBool:
		type = CS_BIT_TYPE; // single bit type
		break;
	case DB_dtShort:
		type = CS_SMALLINT_TYPE; // 2-byte integer type
		break;
	case DB_dtInt:
		type = CS_INT_TYPE; // 4-byte integer type
		break;
	case DB_dtLong:
		type = CS_BIGINT_TYPE; // BIGINT
		break;
	case DB_dtReal:
		type = CS_FLOAT_TYPE; // 8-byte float type
		break;
	case DB_dtNumeric:
		type = CS_NUMERIC_TYPE; // Numeric type
		break;
	case DB_dtBytes:
		type = CS_BINARY_TYPE; // binary type
		break;
	case DB_dtLongBinary:
	case DB_dtBLob:
		type = CS_IMAGE_TYPE; // image type
		break;
	case DB_dtLongChar:
	case DB_dtCLob:
		type = CS_TEXT_TYPE; // text type
		break;
	case DB_dtDateTime:
	default:
		type = CS_CHAR_TYPE; // character type
		break;
	}

	return type;
}

} /* namespace edb */
} /* namespace efc */

//=============================================================================

extern "C" {
	ES_DECLARE(efc::edb::EDatabase*) makeDatabase(efc::edb::EDBProxyInf* proxy)
	{
   		return new efc::edb::EDatabase_freetds(proxy);
	}
}
