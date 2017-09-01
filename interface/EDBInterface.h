/*
 * EDBInterface.hh
 *
 *  Created on: 2017-5-24
 *      Author: cxxjava@163.com
 */

#ifndef __EDBINTERFACE_H__
#define __EDBINTERFACE_H__

//协议版本
#define EDB_INF_VERSION          "1.0"

//协议关键字
#define EDB_KEY_VERSION           "version"
#define EDB_KEY_MSGTYPE           "msgtype"
#define EDB_KEY_CHARSET           "charset"
#define EDB_KEY_DATABASE          "database"
#define EDB_KEY_HOST              "host"
#define EDB_KEY_PORT              "port"
#define EDB_KEY_USERNAME          "username"
#define EDB_KEY_PASSWORD          "password"
#define EDB_KEY_TIMEOUT           "timeout"
#define EDB_KEY_ERRCODE           "errcode"
#define EDB_KEY_ERRMSG            "errmsg"
#define EDB_KEY_DBTYPE            "dbtype"
#define EDB_KEY_DBVERSION         "dbversion"
#define EDB_KEY_TIMESTAMP         "timestamp"
#define EDB_KEY_OFFSET            "offset"
#define EDB_KEY_ROWS              "rows"
#define EDB_KEY_AUTOCOMMIT        "autocommit"
#define EDB_KEY_FETCHSIZE         "fetchsize"
#define EDB_KEY_SQLS              "sqls"
#define EDB_KEY_SQL               "sql"
#define EDB_KEY_OID               "oid"
#define EDB_KEY_CURSOR            "cursor"
#define EDB_KEY_PARAMS            "params"
#define EDB_KEY_PARAM             "param"
#define EDB_KEY_FIELDS            "fields"
#define EDB_KEY_FIELD             "field"
#define EDB_KEY_NAME              "name"
#define EDB_KEY_TYPE              "type"
#define EDB_KEY_LENGTH            "length"
#define EDB_KEY_PRECISION         "precision"
#define EDB_KEY_SCALE             "scale"
#define EDB_KEY_RECORDS           "records"
#define EDB_KEY_RECORD            "record"
#define EDB_KEY_DATASET           "dataset"
#define EDB_KEY_RESULT            "result"
#define EDB_KEY_AFFECTED          "affected"
#define EDB_KEY_WRITTEN           "written"
#define EDB_KEY_READING           "reading"
#define EDB_KEY_RESUME            "resume"

//连接默认编码
#define SQL_DEFAULT_ENCODING      "utf8"

//SQL字符串导引符
#define SQL_STRING_QUOTATION     39    //is ', not "

//SQL占位符表示符
#define SQL_PLACEHOLDER_CHAR     63    //?

//单字段最大尺寸
#define FIELD_MAX_SIZE           104857600       //100M

//SQL操作类型
typedef enum
{
	DB_SQL_DBOPEN         =   0,              //open database
	DB_SQL_DBCLOSE        =   1,              //close database
	DB_SQL_EXECUTE        =   2,              //sql execute, none dataset or multi-datasets
	DB_SQL_UPDATE         =   3,              //sql update, no dataset
	DB_SQL_MORE_RESULT    =   4,              //next resultset
	DB_SQL_RESULT_FETCH   =   5,              //result fetch
	DB_SQL_RESULT_CLOSE   =   6,              //result close
	DB_SQL_SET_AUTOCOMMIT =   7,              //set auto commit flag
	DB_SQL_COMMIT         =   8,              //commit work
	DB_SQL_ROLLBACK       =   9,              //rollback work
	DB_SQL_SETSAVEPOINT   =   10,             //set savepoint
	DB_SQL_BACKSAVEPOINT  =   11,             //rollback savepoint
	DB_SQL_RELESAVEPOINT  =   12,             //release savepoint
	DB_SQL_LOB_CREATE     =   13,             //create large object
	DB_SQL_LOB_WRITE      =   14,             //write large object
	DB_SQL_LOB_READ       =   15,             //read large object
} edb_sql_operation_e;

//字段数据类型
typedef enum
{
	DB_dtUnknown            =   0,                //unknown
	DB_dtString             =   1,                //string
	DB_dtBool               =   2,                //bool
	DB_dtShort              =   3,                //int2
	DB_dtInt                =   4,                //int4
	DB_dtLong               =   5,                //int8
	DB_dtReal               =   6,                //float|double
	DB_dtNumeric            =   8,                //numeric
	DB_dtDateTime           =   9,                //date time
	DB_dtBytes              =   10,               //bytes
	DB_dtLongBinary         =   11,               //long binary
	DB_dtLongChar           =   12,               //long char
	DB_dtBLob               =   13,               //binary lob
	DB_dtCLob               =   14,               //character lob
	DB_dtNull               =   15,               //null
	DB_dtCursor             =   16                //!only for callable statememt output parameter and cursor type
} edb_field_type_e;

#endif /* __EDBINTERFACE_H__ */
