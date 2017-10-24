/*
 * EDatabaseInf.hh
 *
 *  Created on: 2017-6-12
 *      Author: cxxjava@163.com
 */

#ifndef EDATABASEINF_HH_
#define EDATABASEINF_HH_

#include "Efc.hh"

namespace efc {
namespace edb {

/**
 * 数据库插件接口
 */

interface EDatabaseInf: virtual public EObject {
	virtual ~EDatabaseInf() {};

	virtual sp<EBson> processSQL(EBson *req, void *arg) = 0;

	virtual int getErrorCode() = 0;
	virtual EString getErrorMessage() = 0;

	virtual EString getDBType() = 0;

	virtual boolean open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset, int timeout) = 0;
	virtual boolean close() = 0;
};

/**
 * 数据库代理接口
 */
interface EDBProxyInf: virtual public EObject {
	virtual ~EDBProxyInf() {};

	virtual EString getProxyVersion() = 0;
	virtual void dumpSQL(const char *oldSql, const char *newSql) = 0;
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDATABASEINF_HH_ */
