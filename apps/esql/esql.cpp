/**
 * @file  esql.cpp
 * @author cxxjava@163.com
 */

#include "es_main.h"
#include "Edb.hh"
#include <signal.h>
#include <unistd.h>
#include <assert.h>

#define HAVE_READLINE

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* HAVE_READLINE */

static boolean direct = false;
static char *dburl = NULL;
static int istty = 0;

/**
 * ESQL_BASEARGS is the command argument list parsed by getopt() format.
 * -c				 : [-c connect url]
 * -v				 : show version number
 * ?				 : list available command line options
 */
#define ESQL_BASEARGS "c:dv?"

static void show_help(int argc, const char * const *argv)
{
	const char *bin = argv[0];
	
	printf("Usage: %s [-c url]\n", bin);
	printf("       %s [-d] [-v] [?]\n", bin);
	printf("Options:\n"
		   "-c				 : [-c connect url]\n"
		   "-d				 : direct connection\n"
		   "-v				 : show version number\n"
		   "?				 : list available command line options\n");
}

static boolean usage(int argc, const char * const *argv)
{
	int o;
	
	/* Copyright information*/
	printf("==============================\n");
	printf("| Name: esql, Version: 0.1.0 |\n");
	printf("| Author: cxxjava@163.com    |\n");
	printf("| https://github.com/cxxjava |\n");
	printf("==============================\n");
	
	if (argc < 2) {
		show_help(argc, argv);
		return false;
	}
	
	while (-1 != (o = getopt(argc, (char * const *)argv, ESQL_BASEARGS))) {
		switch(o) {
		case 'c':
			if (strlen(optarg) > 0) {
				dburl = strdup(optarg);
			}
			else {
				show_help(argc, argv);
			}
			break;
		case 'd':
			direct = true;
			break;
		case 'v':
			printf("server version: %s\n", "1.0.0");
			return false;
		case '?':
			show_help(argc, argv);
			return false;
		}
	}
	
	return true;
}

static void show_dbinfo(EConnection& dbconn)
{
	printf("dbtype:%s, version:%s\n",
			dbconn.getMetaData()->getDatabaseProductName().c_str(),
			dbconn.getMetaData()->getDatabaseProductVersion().c_str());
}

static char *sql_readline(char *prompt)
{
	size_t sz, pos;
	char *line, *p;

#ifdef HAVE_READLINE
	if (istty)
		return readline(prompt);
#endif

	sz = 1024;
	pos = 0;
	line = (char*) malloc(sz);
	if (!line)
		return NULL;

	if (prompt && prompt[0])
		printf("%s", prompt);
	for (;;) {
		/* read a piece */
		if (fgets(line + pos, sz - pos, stdin) == NULL) {
			if (pos)
				return line;
			break;
		}

		/* got end-of-line ? */
		p = strchr(line + pos, '\n');
		if (p) {
			*p = 0;
			return line;
		}

		/* allocate space if needed */
		pos += strlen(line + pos);
		if (pos + 1024 >= sz) {
			sz += 1024;
			p = (char*) realloc(line, sz);
			if (!p)
				break;
			line = p;
		}
	}
	free(line);
	return NULL;
}

static void sql_add_history(const char *s)
{
#ifdef HAVE_READLINE
	if (istty)
		add_history(s);
#endif
}

static boolean do_sql_exec(EConnection& dbconn, char *sql)
{
	ECommonStatement stmt(&dbconn);

	stmt.setSql(sql);
	stmt.execute();

	printf("execute sql success.\n");

	int rsc = 0;
	EResultSet *rs = stmt.getResultSet();
	while (rs) {
		printf("+=========: %d\n", ++rsc);
		int row = 0;
		while (rs->next()) {
			for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
				printf("%-10s: ", rs->getMetaData()->getColumnLabel(i).c_str());
				printf("[%s]\n", rs->getString(i).c_str());
			}
			printf("+---------: %d\n", ++row);
		}

		rs = stmt.getResultSet(true);
	}
	if (rs) rs->close();
	
	return true;
}

//=============================================================================

static int Main(int argc, const char **argv)
{
	ESystem::init(argc, argv);

	try {
		if (!usage(argc, argv))
			return -1;

		EConnection dbconn(!direct);
		char *s = NULL, *s2 = NULL, *cmd = NULL;
		char prompt[20];
		int line = 0;
		char *mybuf;
		int bufsz = 4096;
		size_t buflen = 0;

#ifndef WIN32
		signal(SIGPIPE, SIG_IGN);
#endif

		try {
			dbconn.connect(dburl);
		} catch (EThrowable& t) {
			printf("connect to %s failed: %s\n", dbconn.getConnectionDatabaseType().c_str(), t.getMessage());
			return -1;
		}

		show_dbinfo(dbconn);
		istty = isatty(0);
		
		/* give the buffer an initial size */
		mybuf = (char *) malloc(bufsz);
		mybuf[0] = '\0';
		
		for (;;) {
			sprintf(prompt, "%d> ", ++line);
			if (s)
				free(s);
			s = sql_readline(prompt);
			if (s == NULL)
				break;

			/*
			 * 'GO' is special only at the start of a line
			 *  The rest of the line may include options that apply to the batch,
			 *  and perhaps whitespace.
			 */
			if (0 == strncasecmp(s, "go", 2) && (strlen(s) == 2 || isspace(s[2]))) {
				line = 0;
				try {
					do_sql_exec(dbconn, mybuf);
				} catch (EThrowable& t) {
					printf("%s\n", t.getMessage());
				}
				mybuf[0] = '\0';
				buflen = 0;
				continue;
			}

			/* skip leading whitespace */
			if (s2)
				free(s2);
			s2 = strdup(s);	/* copy to mangle with strtok() */
			cmd = strtok(s2, " \t");

			if (!cmd)
				cmd = "";

			if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit") || !strcmp(cmd, "bye")) {
				break;
			}
			else {
				while (strlen(mybuf) + strlen(s) + 2 > bufsz) {
					bufsz *= 2;
					mybuf = (char *) realloc(mybuf, bufsz);
				}
				sql_add_history(s);
				strcat(mybuf, s);
				/* preserve line numbering for the parser */
				strcat(mybuf, "\n");
			}
		}
	} catch (EException& e) {
		e.printStackTrace();
	} catch (...) {
	}

	ESystem::exit(0);

	return 0;
}

#ifdef DEBUG
MAIN_IMPL(test_esql) {
	return Main(argc, argv);
}
#else
int main(int argc, const char **argv) {
	return Main(argc, argv);
}
#endif
