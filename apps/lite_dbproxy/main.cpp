/*
 * main.cpp
 *
 *  Created on: 2017-7-17
 *      Author: cxxjava@163.com
 */

#include "es_main.h"
#include "ENaf.hh"
#include "../../interface/EDBInterface.h"
#include "../../dblib/inc/EDatabase.hh"

using namespace efc::edb;

#define PROXY_VERSION "version 0.1.0"

#define DEFAULT_CONNECT_PORT 6633
#define DEFAULT_CONNECT_SSL_PORT 6643

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

extern "C" {
typedef efc::edb::EDatabase* (make_database_t)(ELogger* workLogger,
		ELogger* sqlLogger, const char* clientIP, const char* version);
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
				make_database_t* func = (make_database_t*)eso_dso_sym(handle, "makeDatabase");
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

				//LOG("dbname=%s, ip=%s, port=%d, username=%s, password=%s, charset=%s",
				//		       vdb->database.c_str(), vdb->host.c_str(), vdb->port,
				//		       vdb->username.c_str(), vdb->password.c_str(), vdb->clientEncoding.c_str());

				sp<EDatabase> db = getDatabase(vdb->dbType.c_str(), null, null);
				if (db == null) {
					continue;
				}

				boolean r = db->open(vdb->database.c_str(), vdb->host.c_str(),
						vdb->port, vdb->username.c_str(),
						vdb->password.c_str(), vdb->clientEncoding.c_str(),
						vdb->connectTimeout);
				if (!r) {
					EString msg = EString::formatOf("open database [%s] failed: (%s)",
							   vdb->alias_dbname.c_str(), db->getErrorMessage());
					throw EException(__FILE__, __LINE__, msg.c_str());
				}
				LOG("virtual database [%s] connect success.", vdb->alias_dbname.c_str());
			}
		}}
	}
	sp<EDatabase> getDatabase(const char* dbtype, const char* clientIP, const char* version) {
		EString key(dbtype);
		SoHandle *sh = handles.get(&key);
		if (!sh) return null;
		make_database_t* func = sh->func;
		return func(wlogger.get(), slogger.get(), clientIP, version);
	}
private:
	struct SoHandle: public EObject {
		es_dso_t *handle;
		make_database_t *func;
	};
	EConfig& conf;
	EHashMap<EString*,SoHandle*> handles;
};

class DBProxyServer: public ESocketAcceptor {
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
		ES_ASSERT(acceptor);

		sp<EDatabase> database;

		sp<EHttpRequest> request;
		while(!session->getService()->isDisposed()) {
			// do request read
			try {
				request = dynamic_pointer_cast<EHttpRequest>(session->read());
			} catch (ESocketTimeoutException& e) {
				//LOG("session read timeout.");
				continue;
			} catch (EIOException& e) {
				wlogger->error("session read error.");
				break;
			}
			if (request == null) {
				// client closed.
				if (database != null) {
					database->close();
				}
				wlogger->trace("session client closed.");
				break;
			}

			//req
			EBson req;
			req.Import(request->getBodyData(), request->getBodyLen());

			if (req.getInt(EDB_KEY_MSGTYPE) == DB_SQL_DBOPEN) {
				EString username = req.getString(EDB_KEY_USERNAME);
				QueryUser* user = acceptor->usrmgr.checkUser(
						username.c_str(),
						req.getString(EDB_KEY_PASSWORD).c_str(),
						req.getLLong(EDB_KEY_TIMESTAMP));
				if (!user) {
					EString msg = "username or password error.";
					wlogger->warn(msg.c_str());
					responseFailed(session, msg);
					break;
				}
				VirtualDB* vdb = user->virdb;
				if (!vdb) {
					EString msg("no database access permission.");
					wlogger->warn(msg.c_str());
					responseFailed(session, msg);
					break;
				}
				database = acceptor->somgr.getDatabase(vdb->dbType.c_str(), session->getRemoteAddress()->getHostName(), req.getString(EDB_KEY_CHARSET).c_str());
				if (database == null) {
					EString msg = EString::formatOf("database %s is closed.", vdb->dbType.c_str());
					wlogger->warn(msg.c_str());
					responseFailed(session, msg);
					break;
				}

				//update some field to real data.
				req.set(EDB_KEY_HOST, vdb->host.c_str());
				req.setInt(EDB_KEY_PORT, vdb->port);
				req.set(EDB_KEY_USERNAME, vdb->username.c_str());
				req.set(EDB_KEY_PASSWORD, vdb->password.c_str());
			} else if (database == null) {
				break;
			}

			//交由线程池去执行，以免协程阻塞
			EFiberChannel<EBson> channel(0);
			acceptor->executors->executeX([&database, &req, &channel]() {
				sp<EBson> rep = database->processSQL(&req, null);
				channel.write(rep);
			});

			//rep
			sp<EBson> rep = channel.read();
			ES_ASSERT(rep != null);
			EByteBuffer out;
			rep->Export(&out, null, false);

			// do response write
			session->write(new EHttpResponse(out.data(), out.size()));
		}

		wlogger->trace_("Out of Connection.");
	}

	static void responseFailed(ESocketSession* session, EString& errmsg) {
		sp<EBson> rep = new EBson();
		rep->addInt(EDB_KEY_ERRCODE, ES_FAILURE);
		rep->add(EDB_KEY_ERRMSG, errmsg.c_str());
		EByteBuffer out;
		rep->Export(&out, null, false);
		session->write(new EHttpResponse(out.data(), out.size()));
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
	EHttpCodecFilter hcf;
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

	sa.getFilterChainBuilder()->addLast("http", &hcf);

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
