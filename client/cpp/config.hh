/*
 * config.hh
 *
 *  Created on: 2017-6-13
 *      Author: cxxjava@163.com
 */

#ifndef CONFIG_HH_
#define CONFIG_HH_

/**
 * 功能：edb client 编译模式开关
 * 说明：1-静态链接 | 0-其他模式
 */
#define EDB_CLIENT_STATIC  0

/**
 * 其他模式时，是否为tars服务客户端
 */
#define USE_TARS_CLIENT 1

#endif /* CONFIG_HH_ */
