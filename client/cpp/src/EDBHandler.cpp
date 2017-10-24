/*
 * EDBHandler.cpp
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#include "EUtils.hh"
#include "../inc/EDBHandler.hh"
#include "../inc/EConnection.hh"
#include "../../../interface/EDBInterface.h"

namespace efc {
namespace edb {

extern "C" {
extern efc::edb::EDatabase* makeDatabase(EDBProxyInf* proxy);
typedef efc::edb::EDatabase* (make_database_t)(EDBProxyInf* proxy);
}

static void write_pkg_head(EOutputStream* os, int type, boolean gzip,
		boolean next, int length, uint reqid=0) {
	edb_pkg_head_t pkg_head;
	memset(&pkg_head, 0, sizeof(pkg_head));
	pkg_head.magic = 0xEE;
	pkg_head.type = type;
	pkg_head.gzip = gzip ? 1 : 0;
	pkg_head.next = next ? 1 : 0;
	pkg_head.pkglen = htonl(length);
	pkg_head.reqid = htonl(reqid);

	ECRC32 crc32;
	crc32.update((byte*)&pkg_head, sizeof(pkg_head)-sizeof(pkg_head.crc32));
	pkg_head.crc32 = htonl(crc32.getValue());

	os->write(&pkg_head, sizeof(pkg_head));
}

static void req_stream_read_and_write(EInputStream* is, EOutputStream* os) {
	EA<byte> buf(8192*4); //32k
	int len;
	while ((len = is->read(buf.address(), buf.length())) > 0) {
		EByteArrayOutputStream baos;
		EGZIPOutputStream gos(&baos);
		gos.write(buf.address(), len);
		gos.finish();
		//write head
		write_pkg_head(os, 1, true, true, baos.size());
		//write body
		os->write(baos.data(), baos.size());
	}
	//end stream
	write_pkg_head(os, 1, false, false, 0);
}

static edb_pkg_head_t read_pkg_head(EInputStream* is) {
	edb_pkg_head_t pkg_head;
	char *b = (char*)&pkg_head;
	int len = sizeof(edb_pkg_head_t);
	int n = 0;

	while (n < len) {
		int count = is->read(b + n, len - n);
		if (count < 0)
			throw EEOFEXCEPTION;
		n += count;
	}

	//check falg
	if (pkg_head.magic != 0xEE) {
		throw EIOException(__FILE__, __LINE__, "flag error");
	}

	//check crc32
	ECRC32 crc32;
	crc32.update((byte*)&pkg_head, sizeof(pkg_head)-sizeof(pkg_head.crc32));
	pkg_head.crc32 = ntohl(pkg_head.crc32);
	if (pkg_head.crc32 != crc32.getValue()) {
		throw EIOException(__FILE__, __LINE__, "crc32 error");
	}

	pkg_head.reqid = ntohl(pkg_head.reqid);
	pkg_head.pkglen = ntohl(pkg_head.pkglen);

	return pkg_head;
}

static edb_pkg_head_t read_pkg_head(const void* buf, int size) {
	ES_ASSERT(size >= sizeof(edb_pkg_head_t));

	edb_pkg_head_t pkg_head;
	memcpy(&pkg_head, buf, sizeof(pkg_head));
	pkg_head.reqid = ntohl(pkg_head.reqid);
	pkg_head.pkglen = ntohl(pkg_head.pkglen);
	pkg_head.crc32 = ntohl(pkg_head.crc32);

	return pkg_head;
}

static void rep_stream_read_and_write(EInputStream* is, EOutputStream* os,
		edb_pkg_head_t& pkg_head) {
	EBoundedInputStream bis(is, pkg_head.pkglen);
	char buf[8192];
	int len;
	if (pkg_head.gzip) {
		EGZIPInputStream gis(&bis);
		while ((len = gis.read(buf, sizeof(buf))) > 0) {
			os->write(buf, len);
		}
	} else {
		while ((len = bis.read(buf, sizeof(buf))) > 0) {
			os->write(buf, len);
		}
	}

	//get next
	if (pkg_head.next) {
		edb_pkg_head_t pkg_head = read_pkg_head(is);
		rep_stream_read_and_write(is, os, pkg_head);
	}
}

#if USE_TARS_CLIENT
/*
 响应包解码函数，根据特定格式解码从服务端收到的数据，解析为ResponsePacket
*/
static size_t proxyResponse(const char* recvBuffer, size_t length, list<ResponsePacket>& done)
{
	size_t pos = 0;
	while (pos < length)
	{
		uint32_t len = length - pos;
		if(len < sizeof(edb_pkg_head_t))
		{
			break;
		}

		edb_pkg_head_t pkg_head = read_pkg_head((recvBuffer + pos), sizeof(edb_pkg_head_t));

		//check falg
		if (pkg_head.magic != 0xEE) {
			throw EIOException(__FILE__, __LINE__, "flag error");
		}

		//包没有接收全
		if (len < pkg_head.pkglen + sizeof(edb_pkg_head_t))
		{
			break;
		}
		else
		{
			ResponsePacket rsp;
			rsp.iRequestId = pkg_head.reqid; //!!!
			rsp.sBuffer.resize(pkg_head.pkglen + sizeof(edb_pkg_head_t));
			memcpy(&rsp.sBuffer[0], recvBuffer + pos, pkg_head.pkglen + sizeof(edb_pkg_head_t));

			pos += pkg_head.pkglen + sizeof(edb_pkg_head_t);

			done.push_back(rsp);
		}
	}

	return pos;
}

/*
   请求包编码函数，本函数的打包格式为
   整个包长度（字节）+iRequestId（字节）+包内容
*/
static void proxyRequest(const RequestPacket& request, string& buff)
{
	buff.assign((const char*)(&request.sBuffer[0]), request.sBuffer.size());
}

static sp<EBson> S2R(EBson* req, ServantPrx prx, EObject* arg) {

	//1. req
	uint32_t reqid = prx->tars_gen_requestid();
	EByteArrayOutputStream baos;
	EByteBuffer buf;
	req->Export(&buf, null, false);
	//write head
	write_pkg_head(&baos, 0, false, false, buf.size(), reqid);
	//write body
	baos.write(buf.data(), buf.size());

	ResponsePacket rsp;
	prx->rpc_call(reqid, "dbproxy", (const char*)baos.data(), baos.size(), rsp);

	//2. rep
	sp<EBson> rep;
	{
		EByteArrayInputStream is(rsp.sBuffer.data(), rsp.sBuffer.size());

		//read head
		edb_pkg_head_t pkg_head = read_pkg_head(&is);

		//try to read lob
		if (pkg_head.type == 1) {
			EOutputStream* os = dynamic_cast<EOutputStream*>(arg);
			ES_ASSERT(os);
			rep_stream_read_and_write(&is, os, pkg_head);

			//read message head
			pkg_head = read_pkg_head(&is);
		}

		//read message body
		ES_ASSERT(pkg_head.type == 0);

		rep = new EBson();
		EBoundedInputStream bis(&is, pkg_head.pkglen);
		if (pkg_head.gzip) {
			EGZIPInputStream gis(&bis);
			EBsonParser bp(&gis);
			bp.nextBson(rep.get());
		} else {
			EBsonParser bp(&bis);
			bp.nextBson(rep.get());
		}
	}
	return rep;
}
#else //!
static sp<EBson> S2R(EBson* req, sp<ESocket> socket, EObject* arg) {
	sp<EBson> rep;
	try {
		//1. req
		{
			EByteBuffer buf;
			req->Export(&buf, null, false);
			EOutputStream *os = socket->getOutputStream();

			//write head
			write_pkg_head(os, 0, false, false, buf.size());
			//write body
			os->write(buf.data(), buf.size());

			//write lobs
			if (arg) {
				int msgtype = req->getInt(EDB_KEY_MSGTYPE);
				switch (msgtype) {
				case DB_SQL_EXECUTE:
				case DB_SQL_UPDATE:
				{
					EIterable<EInputStream*>* itb = dynamic_cast<EIterable<EInputStream*>*>(arg);
					ES_ASSERT(itb);
					sp<EIterator<EInputStream*> > iter = itb->iterator();
					while (iter->hasNext()) {
						EInputStream* is = iter->next();
						req_stream_read_and_write(is, os);
					}
					break;
				}
				case DB_SQL_LOB_WRITE: {
					EInputStream* is = dynamic_cast<EInputStream*>(arg);
					ES_ASSERT(is);
					req_stream_read_and_write(is, os);
					break;
				}
				default: break;
				}
			}
		}

		//2. rep
		{
			EInputStream *is = socket->getInputStream();

			//read head
			edb_pkg_head_t pkg_head = read_pkg_head(is);

			//try to read lob
			if (pkg_head.type == 1) {
				EOutputStream* os = dynamic_cast<EOutputStream*>(arg);
				ES_ASSERT(os);
				rep_stream_read_and_write(is, os, pkg_head);

				//read message head
				pkg_head = read_pkg_head(is);
			}

			//read message body
			ES_ASSERT(pkg_head.type == 0);

			rep = new EBson();
			EBoundedInputStream bis(is, pkg_head.pkglen);
			if (pkg_head.gzip) {
				EGZIPInputStream gis(&bis);
				EBsonParser bp(&gis);
				bp.nextBson(rep.get());
			} else {
				EBsonParser bp(&bis);
				bp.nextBson(rep.get());
			}
		}
	} catch (...) {
		// read error
		if (rep == null) {
			rep = new EBson();
			rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
			rep->add(EDB_KEY_ERRMSG, "socket error");
		}
	}
	return rep;
}
#endif //!USE_TARS_CLIENT

//=============================================================================

EDBHandler::~EDBHandler() {
#if USE_TARS_CLIENT
	//
#else
	if (socket != null) {
		socket->close();
	}
#endif
}

EDBHandler::EDBHandler(EConnection *conn) {
	m_Conn = conn;
}

void EDBHandler::genRequestBase(EBson *req, int type, boolean needPwd) {
	llong timestamp = ESystem::currentTimeMillis();
	req->addInt(EDB_KEY_MSGTYPE, type);
	if (needPwd) {
		req->add(EDB_KEY_USERNAME, m_Username.c_str());
		if (m_Conn->isProxyMode()) { //代理模式
			EString sb(m_Username);
			sb.append(m_Password).append(timestamp);
			byte pw[ES_MD5_DIGEST_LEN] = {0};
			es_md5_ctx_t c;
			eso_md5_init(&c);
			eso_md5_update(&c, (const unsigned char *)sb.c_str(), sb.length());
			eso_md5_final((es_uint8_t*)pw, &c);
			req->add(EDB_KEY_PASSWORD, EString::toHexString(pw, ES_MD5_DIGEST_LEN).c_str());
		} else {
			req->add(EDB_KEY_PASSWORD, m_Password.c_str());
		}
	}
	req->add(EDB_KEY_VERSION, EDB_INF_VERSION);
	req->addLLong(EDB_KEY_TIMESTAMP, timestamp);
}

void EDBHandler::open(const char *database, const char *host, int port,
		const char *username, const char *password, const char *charset,
		int timeout) {
	m_Username = username;
	m_Password = password;

#if EDB_CLIENT_STATIC // 直连模式(静态编译)
	this->database = makeDatabase(null);
#else
	if (!m_Conn->isProxyMode()) { // 直连模式(动态加载)
		es_string_t* path = NULL;
		eso_current_workpath(&path);
		EString dsopath(path);
		eso_mfree(path);
		dsopath.append("/dblib/")
#ifdef WIN32
				.append("/win/")
				.append(m_Conn->getConnectionDatabaseType())
				.append(".dll");
#else
#ifdef __APPLE__
				.append("/osx/")
#else //__linux__
				.append("/linux/")
#endif
				.append(m_Conn->getConnectionDatabaseType())
				.append(".so");
#endif
		es_dso_t* handle = eso_dso_load(dsopath.c_str());
		if (!handle) {
			throw EFileNotFoundException(__FILE__, __LINE__, dsopath.c_str());
		}
		make_database_t* func = (make_database_t*)eso_dso_sym(handle, "makeDatabase");
		if (!func) {
			throw ERuntimeException(__FILE__, __LINE__, "makeDatabase");
		}
		this->database = func(null);
	} else { // 代理模式
#if USE_TARS_CLIENT
		if (timeout < 0) timeout = 60; //60s
		_comm.setProperty("stat", "tars.tarsstat.StatObj");
		EString locator = EString::formatOf("tars.tarsregistry.QueryObj@tcp -h %s -p %d -t %d", host, port, timeout*1000);
		_comm.setProperty("locator", locator.c_str());
		_comm.stringToProxy("CxxJava.DBProxyServer.DBProxyServantObj", _prx);

		_prx->tars_set_timeout(timeout*1000); //!

		ProxyProtocol prot;
		prot.requestFunc = proxyRequest;
		prot.responseFunc = proxyResponse;
		_prx->tars_set_protocol(prot);
#else
		if (m_Conn->getSSL()) {
			this->socket = new ESSLSocket();
		} else {
			this->socket = new ESocket();
		}
		this->socket->connect(host, port, timeout);
#endif
	}
#endif

	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_DBOPEN, true);
	req.add(EDB_KEY_DATABASE, database);
	req.add(EDB_KEY_HOST, host);
	req.addInt(EDB_KEY_PORT, port);
	req.add(EDB_KEY_CHARSET, charset);
	req.addInt(EDB_KEY_TIMEOUT, timeout);

	if (!m_Conn->isProxyMode()) {
		rep = this->database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	// set some connection metadata info.
	m_Conn->getMetaData()->productName = rep->get(EDB_KEY_DBTYPE);
	m_Conn->getMetaData()->productVersion = rep->get(EDB_KEY_DBVERSION);
}

void EDBHandler::close() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_DBCLOSE);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
		database.reset();
	} else {
		rep = S2R(&req, socket, null);
#if USE_TARS_CLIENT
		_comm.terminate();
#else
		socket->close();
#endif
	}

	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

boolean EDBHandler::isClosed() {
	if (!m_Conn->isProxyMode()) {
		return !database ? true : false;
	} else {
#if USE_TARS_CLIENT
		//TODO...
		return false;
#else
		return socket->isClosed() ? true : false;
#endif
	}
}

sp<EBson> EDBHandler::executeSQL(EBson *req, EIterable<EInputStream*>* itb) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, itb);
	} else {
		return S2R(req, socket, itb);
	}
}

sp<EBson> EDBHandler::updateSQL(EBson *req, EIterable<EInputStream*>* itb) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, itb);
	} else {
		return S2R(req, socket, itb);
	}
}

sp<EBson> EDBHandler::moreResult(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket, null);
	}
}

sp<EBson> EDBHandler::resultFetch(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket, null);
	}
}

sp<EBson> EDBHandler::resultClose(EBson *req) {
	if (!m_Conn->isProxyMode()) {
		return database->processSQL(req, NULL);
	} else {
		return S2R(req, socket, null);
	}
}

void EDBHandler::commit() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_COMMIT);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::rollback() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_ROLLBACK);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::setAutoCommit(boolean autoCommit) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_SET_AUTOCOMMIT);

	req.addByte(EDB_KEY_AUTOCOMMIT, autoCommit ? 1 : 0);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::setSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_SETSAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::rollbackSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_BACKSAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

void EDBHandler::releaseSavepoint(const char* name) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_RELESAVEPOINT);

	req.add(EDB_KEY_NAME, name);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}
}

llong EDBHandler::createLargeObject() {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_CREATE, true);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, NULL);
	} else {
		rep = S2R(&req, socket, null);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_OID);
}

llong EDBHandler::writeLargeObject(llong oid, EInputStream* is) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_WRITE, true);

	req.addLLong(EDB_KEY_OID, oid);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, is);
	} else {
		rep = S2R(&req, socket, is);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_WRITTEN);
}

llong EDBHandler::readLargeObject(llong oid, EOutputStream* os) {
	EBson req;
	sp<EBson> rep;
	genRequestBase(&req, DB_SQL_LOB_READ, true);

	req.addLLong(EDB_KEY_OID, oid);

	if (!m_Conn->isProxyMode()) {
		rep = database->processSQL(&req, os);
	} else {
		rep = S2R(&req, socket, os);
	}
	if (rep->getInt(EDB_KEY_ERRCODE) != ES_SUCCESS) {
		throw ESQLException(__FILE__, __LINE__, rep->get(EDB_KEY_ERRMSG));
	}

	return rep->getLLong(EDB_KEY_READING);
}

} /* namespace edb */
} /* namespace efc */
