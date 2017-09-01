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
	virtual const char* getErrorMessage() = 0;

	virtual const char* getProxyVersion() = 0;

	virtual boolean open(const char *database, const char *host, int port,
			const char *username, const char *password, const char *charset, int timeout) = 0;
	virtual boolean close() = 0;
};

} /* namespace edb */
} /* namespace efc */
#endif /* EDATABASEINF_HH_ */
