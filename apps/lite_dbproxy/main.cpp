/*
 * main.cpp
 *
 *  Created on: 2017-7-17
 *      Author: cxxjava@163.com
 */

#include "es_main.h"
#include "ENaf.hh"
#include "EUtils.hh"
#include "../../interface/EDBInterface.h"
#include "../../dblib/inc/EDatabase.hh"

using namespace efc::edb;

#define PROXY_VERSION "version 0.1.0"

#define DEFAULT_CONNECT_PORT 6633
#define DEFAULT_CONNECT_SSL_PORT 6643

#define DBLIB_STATIC 0

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

extern "C" {
extern efc::edb::EDatabase* makeDatabase(EDBProxyInf* proxy);
typedef efc::edb::EDatabase* (make_database_t)(EDBProxyInf* proxy);
}

static sp<ELogger> wlogger;
static sp<ELogger> slogger;

//=============================================================================

struct VirtualDB: public EObject {
	EString alias_dbname;

	int connectTimeout;
	int socketTimeout;
	EString dbType;
	EString clientEncoding;
	EString database;
	EString host;
	int port;
	EString username;
	EString password;

	VirtualDB(EString& alias, int ct, int st, EString& dt, EString& ce, EString& db, EString& h, int port, EString& usr, EString& pwd) :
		alias_dbname(alias),
		connectTimeout(ct),
		socketTimeout(st),
		dbType(dt),
		clientEncoding(ce),
		database(db),
		host(h),
		port(port),
		username(usr),
		password(pwd) {
	}
};

struct QueryUser: public EObject {
	EString username;
	EString password;
	VirtualDB *virdb;

	QueryUser(const char* username, const char* password, VirtualDB* vdb):
		username(username), password(password), virdb(vdb) {
	}
};

class VirtualDBManager: public EObject {
public:
	VirtualDBManager(EConfig& conf): conf(conf) {
		EConfig* subc = conf.getConfig("DBLIST");
		if (!subc) {
			throw EIllegalArgumentException(__FILE__, __LINE__, "[DBLIST] is null.");
		}
		EArray<EString*> usrs = subc->keyNames();
		for (int i=0; i<usrs.size(); i++) {
			const char* alias_dbname = usrs[i]->c_str();
			EString url = subc->getString(alias_dbname);
			VirtualDB* vdb = parseUrl(alias_dbname, url.c_str());
			if (vdb) {
				virdbMap.put(new EString(alias_dbname), vdb);
			}
		}
	}
	EHashMap<EString*,VirtualDB*>& getDBMap() {
		return virdbMap;
	}
	VirtualDB* getVirtualDB(EString* alias_dbname) {
		return virdbMap.get(alias_dbname);
	}
private:
	EConfig& conf;
	EHashMap<EString*,VirtualDB*> virdbMap;

	VirtualDB* parseUrl(EString alias_dbname, const char* url) {
		if (!url || !*url) {
			throw ENullPointerException(__FILE__, __LINE__);
		}

		if (eso_strncmp(url, "edbc:", 5) != 0) {
			throw EProtocolException(__FILE__, __LINE__, "edbc:");
		}

		//edbc:[DBTYPE]://host:port/database?connectTimeout=xxx&socketTimeout=xxx&clientEncoding=xxx&username=xxx&password=xxx

		EURI uri(url + 5 /*edbc:*/);
		EString dbType = uri.getScheme();
		EString host = uri.getHost();
		int port = uri.getPort();
		EString database = uri.getPath();
		database = database.substring(1); //index 0 is '/'
		int connectTimeout = 30;
		int socketTimeout = 30;
		EString clientEncoding("utf8");
		EString ct = uri.getParameter("connectTimeout");
		if (!ct.isEmpty()) {
			connectTimeout = EInteger::parseInt(ct.c_str());
		}
		EString st = uri.getParameter("socketTimeout");
		if (!st.isEmpty()) {
			socketTimeout = EInteger::parseInt(st.c_str());
		}
		EString ce = uri.getParameter("clientEncoding");
		if (!ce.isEmpty()) {
			clientEncoding = ce;
		}

		if (database.isEmpty() || host.isEmpty() || database.isEmpty()) {
			throw EIllegalArgumentException(__FILE__, __LINE__, "url");
		}
		if (port == -1) {
			port = DEFAULT_CONNECT_PORT;
		}
		EString username = uri.getParameter("username");
		EString password = uri.getParameter("password");

		return new VirtualDB(alias_dbname, connectTimeout, socketTimeout, dbType,
				clientEncoding, database, host, port, username, password);
	}
};

class VirtualUserManager: public EObject {
public:
	VirtualUserManager(EConfig& conf, VirtualDBManager& dbs): conf(conf), dbs(dbs) {
		EConfig* subc = conf.getConfig("USERMAP");
		if (!subc) {
			throw EIllegalArgumentException(__FILE__, __LINE__, "[USERMAP] is null.");
		}
		EArray<EString*> usrs = subc->keyNames();
		for (int i=0; i<usrs.size(); i++) {
			const char* username = usrs[i]->c_str();
			EString value = subc->getString(username);
			EArray<EString*> ss = EPattern::split(",", value.c_str());
			if (ss.size() >= 2) {
				userMap.put(new EString(username), new QueryUser(username, ss[1]->c_str(), dbs.getVirtualDB(ss[0])));
			}
		}
	}
	EHashMap<EString*,QueryUser*>& getUserMap() {
		return userMap;
	}
	QueryUser* checkUser(const char* username, const char* password, llong timestamp) {
		EString u(username);
		QueryUser* user = userMap.get(&u);

		if (user) {
			u.append(user->password).append(timestamp);
			byte pw[ES_MD5_DIGEST_LEN] = {0};
			es_md5_ctx_t c;
			eso_md5_init(&c);
			eso_md5_update(&c, (const unsigned char *)u.c_str(), u.length());
			eso_md5_final((es_uint8_t*)pw, &c);

			if (EString::toHexString(pw, ES_MD5_DIGEST_LEN).equalsIgnoreCase(password)) {
				return user;
			}
		}
		return null;
	}
private:
	EConfig& conf;
	VirtualDBManager& dbs;
	EHashMap<EString*,QueryUser*> userMap;
};

class DBSoHandleManager: public EObject {
public:
	~DBSoHandleManager() {
		soCloseAll();
	}
	DBSoHandleManager(EConfig& conf): conf(conf) {
		EConfig* subc = conf.getConfig("DBTYPE");
		if (!subc) {
			throw EIllegalArgumentException(__FILE__, __LINE__, "[DBTYPE] is null.");
		}
	}
	void soOpenAll() {
		es_string_t* path = NULL;
		eso_current_workpath(&path);
		EString dsopath(path);
		eso_mfree(path);

		EConfig* subc = conf.getConfig("DBTYPE");
		EArray<EString*> bdb = subc->keyNames();
		for (int i=0; i<bdb.size(); i++) {
			const char* dbtype = bdb[i]->c_str();
			boolean on = subc->getBoolean(dbtype, false);
			if (on) {
				EString dsofile(dsopath);
				dsofile.append("/dblib/")
#ifdef __APPLE__
						.append("/osx/")
#else //__linux__
						.append("/linux/")
#endif
						.append(dbtype).append(".so");
				es_dso_t* handle = eso_dso_load(dsofile.c_str());
				if (!handle) {
					throw EFileNotFoundException(__FILE__, __LINE__, dsofile.c_str());
				}
#if DBLIB_STATIC
				make_database_t* func = (make_database_t*)makeDatabase;
#else
				make_database_t* func = (make_database_t*)eso_dso_sym(handle, "makeDatabase");
#endif
				if (!func) {
					eso_dso_unload(&handle);
					throw ERuntimeException(__FILE__, __LINE__, "makeDatabase");
				}

				SoHandle *sh = new SoHandle();
				sh->handle = handle;
				sh->func = func;
				handles.put(new EString(dbtype), sh);
			}
		}
	}
	void soCloseAll() {
		sp<EIterator<SoHandle*> > iter = handles.values()->iterator();
		while (iter->hasNext()) {
			SoHandle* sh = iter->next();
			eso_dso_unload(&sh->handle);
		}
		handles.clear();
	}
	void testConnectAll() {
		ON_FINALLY_NOTHROW(
			soCloseAll();
		) {
			soOpenAll();

			LOG("virtual database connecting test...");

			VirtualDBManager dbmgr(conf);
			EHashMap<EString*,VirtualDB*>& dbmap = dbmgr.getDBMap();
			sp<EIterator<VirtualDB*> > iter = dbmap.values()->iterator();
			while (iter->hasNext()) {
				VirtualDB* vdb = iter->next();

				sp<EDatabase> db = getDatabase(vdb->dbType.c_str(), null);
				if (db == null) {
					continue;
				}

				boolean r = db->open(vdb->database.c_str(), vdb->host.c_str(),
						vdb->port, vdb->username.c_str(),
						vdb->password.c_str(), vdb->clientEncoding.c_str(),
						vdb->connectTimeout);
				if (!r) {
					EString msg = EString::formatOf("open database [%s] failed: (%s)",
							   vdb->alias_dbname.c_str(), db->getErrorMessage().c_str());
					throw EException(__FILE__, __LINE__, msg.c_str());
				}
				LOG("virtual database [%s] connect success.", vdb->alias_dbname.c_str());
			}
		}}
	}
	sp<EDatabase> getDatabase(const char* dbtype, EDBProxyInf* proxy) {
		EString key(dbtype);
		SoHandle *sh = handles.get(&key);
		if (!sh) return null;
		make_database_t* func = sh->func;
		return func(proxy);
	}
private:
	struct SoHandle: public EObject {
		es_dso_t *handle;
		make_database_t *func;
	};
	EConfig& conf;
	EHashMap<EString*,SoHandle*> handles;
};

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

static void write_pkg_head(EOutputStream* os, int type,
		boolean next, int length, uint reqid=0) {
	edb_pkg_head_t pkg_head;
	memset(&pkg_head, 0, sizeof(pkg_head));
	pkg_head.magic = 0xEE;
	pkg_head.type = type;
	pkg_head.gzip = 0;
	pkg_head.next = next ? 1 : 0;
	pkg_head.pkglen = htonl(length);
	pkg_head.reqid = htonl(reqid);

	ECRC32 crc32;
	crc32.update((byte*)&pkg_head, sizeof(pkg_head)-sizeof(pkg_head.crc32));
	pkg_head.crc32 = htonl(crc32.getValue());

	os->write(&pkg_head, sizeof(pkg_head));
}

class PackageInputStream: public EInputStream {
public:
	PackageInputStream(EInputStream* cis): cis(cis) {
	}
	virtual int read(void *b, int len) THROWS(EIOException) {
		if (bis == null) {
			head = read_pkg_head(cis);
			if (head.gzip) {
				tis = new EBoundedInputStream(cis, head.pkglen);
				bis = new EGZIPInputStream(tis.get());
			} else {
				bis = new EBoundedInputStream(cis, head.pkglen);
			}
		}
		int r = bis->read(b, len);
		if ((r == -1 || r == 0) && head.next) { //next
			head = read_pkg_head(cis);
			if (head.pkglen > 0) {
				if (head.gzip) {
					tis = new EBoundedInputStream(cis, head.pkglen);
					bis = new EGZIPInputStream(tis.get());
				} else {
					bis = new EBoundedInputStream(cis, head.pkglen);
				}
				return bis->read(b, len);
			} else {
				return -1; //EOF
			}
		} else {
			return r;
		}
	}
private:
	 EInputStream* cis;
	 sp<EInputStream> tis;
	 sp<EInputStream> bis;
	 edb_pkg_head_t head;
};

#define MAX_PACKAGE_SIZE 32*1024 //32k
class PackageOutputStream: public EOutputStream {
public:
	PackageOutputStream(EOutputStream* cos): cos(cos), buf(MAX_PACKAGE_SIZE + 8192) {
	}
	virtual void write(const void *b, int len) THROWS(EIOException) {
		if (b && len > 0) {
			buf.append(b, len);
			if (buf.size() >= MAX_PACKAGE_SIZE) {
				EByteArrayOutputStream baos;
				EGZIPOutputStream gos(&baos);
				gos.write(buf.data(), buf.size());
				gos.finish();
				//write head
				write_pkg_head(cos, 1, true, true, baos.size());
				//write body
				cos->write(baos.data(), baos.size());

				//reset buf
				buf.clear();
			}
		} else {
			//end stream
			if (buf.size() > 0) {
				EByteArrayOutputStream baos;
				EGZIPOutputStream gos(&baos);
				gos.write(buf.data(), buf.size());
				gos.finish();
				//write head
				write_pkg_head(cos, 1, true, false, baos.size());
				//write body
				cos->write(baos.data(), baos.size());

				//reset buf
				buf.clear();
			} else {
				write_pkg_head(cos, 1, false, false, 0);
			}
		}
	}
private:
	EOutputStream* cos;
	EByteBuffer buf;
};

class DBProxyServer: public ESocketAcceptor, virtual public EDBProxyInf {
public:
	~DBProxyServer() {
		somgr.soCloseAll();
		executors->shutdown();
		executors->awaitTermination();
		delete executors;
	}
	DBProxyServer(EConfig& conf) :
			conf(conf), somgr(conf), dbmgr(conf), usrmgr(conf, dbmgr) {
		somgr.soOpenAll();
		executors = EExecutors::newCachedThreadPool();
	}

	static void onConnection(ESocketSession* session, ESocketAcceptor::Service* service) {
		wlogger->trace_("onConnection: service=%s, client=%s", service->toString().c_str(),
				session->getRemoteAddress()->getAddress()->getHostAddress().c_str());

		DBProxyServer* acceptor = dynamic_cast<DBProxyServer*>(session->getService());
		sp<EDatabase> database;

		while(!session->getService()->isDisposed()) {
			sp<ESocket> socket = session->getSocket();
			EInputStream* cis = socket->getInputStream();
			EOutputStream* cos = socket->getOutputStream();

			//req
			EBson req;
			try {
				edb_pkg_head_t pkg_head = read_pkg_head(cis);
				if (pkg_head.type != 0) { //非主请求
					wlogger->error("request error.");
					break;
				}
				EBoundedInputStream bis(cis, pkg_head.pkglen);
				if (pkg_head.gzip) {
					EGZIPInputStream gis(&bis);
					EBsonParser bp(&gis);
					bp.nextBson(&req);
				} else {
					EBsonParser bp(&bis);
					bp.nextBson(&req);
				}
			} catch (ESocketTimeoutException& e) {
				continue;
			} catch (EEOFException& e) {
				wlogger->trace("session client closed.");
				break;
			} catch (EIOException& e) {
				wlogger->error("session read error.");
				break;
			}

			//db open.
			EString errmsg("unknown error.");
			if (req.getInt(EDB_KEY_MSGTYPE) == DB_SQL_DBOPEN) {
				database = doDBOpen(session, req, errmsg);
			}

			if (database == null) {
				wlogger->warn(errmsg.c_str());
				responseFailed(cos, errmsg);
				break;
			}

			//交由线程池去执行，以免协程阻塞
			EFiberChannel<EBson> channel(0);
			acceptor->executors->executeX([&database, &req, &channel, cis, cos, socket]() {
				//协程时自动切换到非阻塞模式，这里要显示设置到阻塞模式。
				ENetWrapper::configureBlocking(socket->getFD(), true);

				sp<EBson> rep;
				int opt = req.getInt(EDB_KEY_MSGTYPE);
				if (opt == DB_SQL_EXECUTE || opt == DB_SQL_UPDATE) {
					class BoundedIterator: public EIterator<EInputStream*> {
					public:
						BoundedIterator(EInputStream* cis): cis(cis) {}
						virtual boolean hasNext() {
							return true;
						}
						virtual EInputStream* next() {
							iss = new PackageInputStream(cis);
							return iss.get();
						}
						virtual void remove() { }
						virtual EInputStream* moveOut() { return null; }
					private:
						 EInputStream* cis;
						 sp<EInputStream> iss;
					};
					class BoundedIterable: public EIterable<EInputStream*> {
					public:
						BoundedIterable(EInputStream* cis): cis(cis) {}
						 virtual sp<EIterator<EInputStream*> > iterator(int index=0) {
							 return new BoundedIterator(cis);
						 }
					private:
						 EInputStream* cis;
					};

					BoundedIterable bi(cis);
					rep = database->processSQL(&req, &bi);
				} else if (opt == DB_SQL_LOB_WRITE) {
					PackageInputStream pis(cis);
					rep = database->processSQL(&req, &pis);
				} else if (opt == DB_SQL_LOB_READ) {
					PackageOutputStream pos(cos);
					rep = database->processSQL(&req, &pos);
				} else {
					rep = database->processSQL(&req, null);
				}

				//恢复状态
				ENetWrapper::configureBlocking(socket->getFD(), false);

				channel.write(rep);
			});

			//rep
			sp<EBson> rep = channel.read();
			ES_ASSERT(rep != null);
			EByteBuffer buf;
			rep->Export(&buf, null, false);

			write_pkg_head(cos, 0, false, buf.size());
			cos->write(buf.data(), buf.size());
		}

		if (database != null) {
			database->close();
		}

		wlogger->trace_("Out of Connection.");
	}

	static sp<EDatabase> doDBOpen(ESocketSession* session, EBson& req, EString& errmsg) {
		DBProxyServer* acceptor = dynamic_cast<DBProxyServer*>(session->getService());

		EString username = req.getString(EDB_KEY_USERNAME);
		QueryUser* user = acceptor->usrmgr.checkUser(
				username.c_str(),
				req.getString(EDB_KEY_PASSWORD).c_str(),
				req.getLLong(EDB_KEY_TIMESTAMP));
		if (!user) {
			errmsg = "username or password error.";
			return null;
		}
		VirtualDB* vdb = user->virdb;
		if (!vdb) {
			errmsg = "no database access permission.";
			return null;
		}

		sp<EDatabase> database = acceptor->somgr.getDatabase(vdb->dbType.c_str(), dynamic_cast<EDBProxyInf*>(session->getService()));

		//update some field to real data.
		req.set(EDB_KEY_HOST, vdb->host.c_str());
		req.setInt(EDB_KEY_PORT, vdb->port);
		req.set(EDB_KEY_USERNAME, vdb->username.c_str());
		req.set(EDB_KEY_PASSWORD, vdb->password.c_str());

		return database;
	}

	static void responseFailed(EOutputStream* cos, EString& errmsg) {
		sp<EBson> rep = new EBson();
		rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
		rep->add(EDB_KEY_ERRMSG, errmsg.c_str());
		EByteBuffer buf;
		rep->Export(&buf, null, false);
		write_pkg_head(cos, 0, false, buf.size());
		cos->write(buf.data(), buf.size());
	}

	virtual EString getProxyVersion() {
		return PROXY_VERSION;
	}

	virtual void dumpSQL(const char *oldSql, const char *newSql) {
		if (slogger != null && (oldSql || newSql)) {
			slogger->log(null, -1, ELogger::LEVEL_INFO, newSql ? newSql : oldSql, null);
		}
	}

private:
	EConfig& conf;
	DBSoHandleManager somgr;
	VirtualDBManager dbmgr;
	VirtualUserManager usrmgr;
	EExecutorService* executors;
};

static void startDBProxy(EConfig& conf) {
	DBProxyServer sa(conf);

	EBlacklistFilter blf;
	EWhitelistFilter wlf;
	EConfig* subconf;

	subconf = conf.getConfig("BLACKLIST");
	if (subconf) {
		EArray<EString*> disallows = subconf->keyNames();
		for (int i=0; i<disallows.size(); i++) {
			blf.block(disallows[i]->c_str());
		}
		sa.getFilterChainBuilder()->addFirst("black", &blf);
	}

	subconf = conf.getConfig("WHITELIST");
	if (subconf) {
		EArray<EString*> allows = subconf->keyNames();
		for (int i=0; i<allows.size(); i++) {
			wlf.allow(allows[i]->c_str());
		}
		sa.getFilterChainBuilder()->addFirst("white", &wlf);
	}

	sa.setConnectionHandler(DBProxyServer::onConnection);
	sa.setMaxConnections(conf.getInt("COMMON/max_connections", 10000));
	sa.setSoTimeout(conf.getInt("COMMON/socket_timeout", 3) * 1000);

	//ssl
	EString ssl_cert = conf.getString("COMMON/ssl_cert");
	EString ssl_key = conf.getString("COMMON/ssl_key");
	if (!ssl_cert.isEmpty()) {
		sa.setSSLParameters(null,
				ssl_cert.c_str(),
				ssl_key.c_str(),
				null, null);
	}

	EArray<EConfig*> confs = conf.getConfigs("SERVICE");
	for (int i=0; i<confs.size(); i++) {
		boolean ssl = confs[i]->getBoolean("ssl_active", false);
		int port = confs[i]->getInt("listen_port", ssl ? DEFAULT_CONNECT_SSL_PORT : DEFAULT_CONNECT_PORT);
		LOG("listen port: %d", port);
		sa.bind("0.0.0.0", port, ssl);
	}

	sa.listen();
}

/**
 * Usage: dbproxy -c [config file]
 *                +d : daemon mode
 *                +c : show cofing sample
 *                +h : show help
 *                +v : show version
 * example: dbproxy -c dbproxy.ini +d"
 */
static void Usage(int flag) {
	if (flag == 0) {
		LOG("Usage: %s -c [config file]", ESystem::getExecuteFilename());
		LOG("        +d : daemon mode");
		LOG("        +c : show cofing sample");
		LOG("        +h : show help");
		LOG("        +v : show version");
	} else {
		LOG("#dbproxy config sample:");
		LOG("[COMMON]");
		LOG("#最大连接数");
		LOG("max_connections = 100");
		LOG("#SO超时时间：秒");
		LOG("socket_timeout = 3");
		LOG("#SSL证书设置");
		LOG("ssl_cert = \"../test/certs/tests-cert.pem\"");
		LOG("ssl_key = \"../test/certs/tests-key.pem\"");
		LOG("[SERVICE]");
		LOG("#服务端口号");
		LOG("listen_port = 6633");
		LOG("[SERVICE]");
		LOG("#服务端口号");
		LOG("listen_port = 6643");
		LOG("#SSL加密");
		LOG("ssl_active = TRUE");
		LOG("");
		LOG("[WHITELIST]");
		LOG("#白名单");
		LOG("127.0.0.1");
		LOG("");
		LOG("[BLACKLIST]");
		LOG("#黑名单");
		LOG("192.168.0.199");
		LOG("");
		LOG("[DBTYPE]");
		LOG("#支持的数据库类型: 类型=true(开)|false(关)");
		LOG("MYSQL=TRUE");
		LOG("PGSQL=TRUE");
		LOG("");
		LOG("[USERMAP]");
		LOG("#客户端访问用户列表: 访问用户名=数据库别名,访问密码");
		LOG("username=alias_dbname,password");
		LOG("");
		LOG("[DBLIST]");
		LOG("#服务数据库列表: 数据库别名=数据库访问URL");
		LOG("alias_dbname = \"edbc:[dbtype]://[host:port]/[database]?connectTimeout=[seconds]&username=[xxx]&password=[xxx]\"");
	}
}

static int Main(int argc, const char **argv) {
	ESystem::init(argc, argv);
	ELoggerManager::init("log4e.properties");

	wlogger = ELoggerManager::getLogger("work");
	slogger = ELoggerManager::getLogger("sql");

	boolean show = false;

	if (ESystem::containsProgramArgument("+h")) {
		show = true;
		Usage(0);
	}

	if (ESystem::containsProgramArgument("+v")) {
		show = true;
		LOG(PROXY_VERSION);
	}

	if (ESystem::containsProgramArgument("+c")) {
		show = true;
		Usage(1);
	}

	if (show) {
		return 0;
	}

	try {
		EString s;

		//daemon
		boolean daemon = ESystem::containsProgramArgument("+d");

		//conf
		EConfig conf;
		s = ESystem::getProgramArgument("c");
		if (!s.isEmpty()) {
			conf.loadFromINI(s.c_str());
		}

		if (conf.isEmpty()) {
			Usage(0);
			return -1;
		}
		//LOG(conf.toString().c_str());

		//test db connection
		DBSoHandleManager somgr(conf);
		somgr.testConnectAll();

		//start server
		if (daemon) {
			ESystem::detach(true, 0, 0, 0);
		}
		startDBProxy(conf);
	}
	catch (EException& e) {
		e.printStackTrace();
	}
	catch (...) {
	}

	ESystem::exit(0);

	return 0;
}


#ifdef DEBUG
MAIN_IMPL(testedb_lite_dbproxy) {
	return Main(argc, argv);
}
#else
int main(int argc, const char **argv) {
	return Main(argc, argv);
}
#endif
