/*
 * ESavepoint.cpp
 *
 *  Created on: 2017-7-4
 *      Author: cxxjava@163.com
 */

#include "../inc/ESavepoint.hh"

namespace efc {
namespace edb {

EAtomicCounter ESavepoint::IDGenerator;

ESavepoint::ESavepoint(const char* name) {
	if (name && *name) {
		_isValid = true;
		_isNamed = true;
		_name = name;
		_id = 0;
	} else {
		_isValid = true;
		_isNamed = false;
		_id = ++IDGenerator;
	}
}

int ESavepoint::getSavepointId() {
	if (!_isValid) {
	  throw ESQLException(__FILE__, __LINE__, "Cannot reference a savepoint after it has been released.");
	}

	if (_isNamed) {
	  throw ESQLException(__FILE__, __LINE__, "Cannot retrieve the id of a named savepoint.");
	}

	return _id;
}

EString ESavepoint::getSavepointName() {
	if (!_isValid) {
	  throw ESQLException(__FILE__, __LINE__, "Cannot reference a savepoint after it has been released.");
	}

	if (!_isNamed) {
	  return EString::formatOf("SAVEPOINT_%d", _id);
	}

	return _name;
}

void ESavepoint::invalidate() {
	_isValid = false;
}

} /* namespace edb */
} /* namespace efc */
