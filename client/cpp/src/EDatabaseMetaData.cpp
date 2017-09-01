/*
 * EDatabaseMetaData.cpp
 *
 *  Created on: 2017-6-7
 *      Author: cxxjava@163.com
 */

#include "../inc/EDatabaseMetaData.hh"

namespace efc {
namespace edb {

EString EDatabaseMetaData::getDatabaseProductName() {
	return productName;
}

EString EDatabaseMetaData::getDatabaseProductVersion() {
	return productVersion;
}

} /* namespace edb */
} /* namespace efc */
