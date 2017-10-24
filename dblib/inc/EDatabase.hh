/*
 * EDatabase.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef EDATABASE_HH_
#define EDATABASE_HH_

#include "Efc.hh"
#include "../../interface/EDatabaseInf.hh"

namespace efc {
namespace edb {

/**
 * 数据库插件基类
 */

abstract class EDatabase: virtual public EDatabaseInf {
public:
	EDatabase(EDBProxyInf* proxy);
	virtual ~EDatabase();

	virtual sp<EBson> processSQL(EBson *req, void *arg);

	virtual void setErrorCode(int errcode);
	virtual int getErrorCode();

	virtual void setErrorMessage(const char* message);
	virtual EString getErrorMessage();

	virtual EString getDBType();

	virtual boolean open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset, int timeout) = 0;
	virtual boolean close() = 0;

protected:
	boolean getAutoCommit();

	llong newCursorID();
	llong currCursorID();

	sp<EBson> genRspCommSuccess();
	sp<EBson> genRspCommFailure();

	void dumpSQL(const char *oldSql, const char *newSql=NULL);

protected:
	EDBProxyInf* m_DBProxy; //代理服务器对象

	boolean m_AutoCommit; //是否自动提交事务
	int m_ErrorCode; //错误代码
	EString m_ErrorMessage; //SQL错误消息

private:
	llong m_CursorID; //Cursor ID

	sp<EBson> onOpen(EBson *req);
	sp<EBson> onClose(EBson *req);

	virtual sp<EBson> onExecute(EBson *req, EIterable<EInputStream*>* itb) = 0;
	virtual sp<EBson> onUpdate(EBson *req, EIterable<EInputStream*>* itb) = 0;
	virtual sp<EBson> onMoreResult(EBson *req) = 0;
    virtual sp<EBson> onResultFetch(EBson *req) = 0;
    virtual sp<EBson> onResultClose(EBson *req) = 0;
	virtual sp<EBson> setAutoCommit(boolean autoCommit) = 0;
	virtual sp<EBson> onCommit() = 0;
	virtual sp<EBson> onRollback() = 0;
	virtual sp<EBson> setSavepoint(EBson *req);
	virtual sp<EBson> rollbackSavepoint(EBson *req);
	virtual sp<EBson> releaseSavepoint(EBson *req);
	virtual sp<EBson> onLOBCreate() = 0;
	virtual sp<EBson> onLOBWrite(llong oid, EInputStream *is) = 0;
	virtual sp<EBson> onLOBRead(llong oid, EOutputStream *os) = 0;

	virtual EString dbtype() = 0;
	virtual EString dbversion() = 0;

	sp<EBson> doSavepoint(EBson *req, EString& sql);

#ifdef DEBUG
	void showMessage(EBson* bson);
#endif
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDATABASE_HH_ */
