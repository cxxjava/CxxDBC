/**
 * @file  esql.cpp
 * @author cxxjava@163.com
 */

#include "es_main.h"
#include "Edb.hh"
#include <signal.h>
#include <assert.h>

#ifdef WIN32
#include <io.h>
#define ISTTY(x) _isatty(x)
#else //!
#include <unistd.h>
#define ISTTY(x) isatty(x)
#define HAVE_READLINE
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* HAVE_READLINE */

#ifdef WIN32
static const char* ID = "$Id: getopt.c,v 1.2 2003/10/26 03:10:20 vindaci Exp $";

static char* optarg = NULL;
static int optind = 0;
static int opterr = 1;
static int optopt = '?';

static char** prev_argv = NULL;        /* Keep a copy of argv and argc to */
static int prev_argc = 0;              /*    tell if getopt params change */
static int argv_index = 0;             /* Option we're checking */
static int argv_index2 = 0;            /* Option argument we're checking */
static int opt_offset = 0;             /* Index into compounded "-option" */
static int dashdash = 0;               /* True if "--" option reached */
static int nonopt = 0;                 /* How many nonopts we've found */

static void increment_index()
{
	/* Move onto the next option */
	if(argv_index < argv_index2)
	{
		while(prev_argv[++argv_index] && prev_argv[argv_index][0] != '-'
				&& argv_index < argv_index2+1);
	}
	else argv_index++;
	opt_offset = 1;
}

/*
* Permutes argv[] so that the argument currently being processed is moved
* to the end.
*/
static int permute_argv_once()
{
	/* Movability check */
	if(argv_index + nonopt >= prev_argc) return 1;
	/* Move the current option to the end, bring the others to front */
	else
	{
		char* tmp = prev_argv[argv_index];

		/* Move the data */
		memmove(&prev_argv[argv_index], &prev_argv[argv_index+1],
				sizeof(char**) * (prev_argc - argv_index - 1));
		prev_argv[prev_argc - 1] = tmp;

		nonopt++;
		return 0;
	}
}

int getopt(int argc, char** argv, char* optstr)
{
	int c = 0;

	/* If we have new argv, reinitialize */
	if(prev_argv != argv || prev_argc != argc)
	{
		/* Initialize variables */
		prev_argv = argv;
		prev_argc = argc;
		argv_index = 1;
		argv_index2 = 1;
		opt_offset = 1;
		dashdash = 0;
		nonopt = 0;
	}

	/* Jump point in case we want to ignore the current argv_index */
	getopt_top:

	/* Misc. initializations */
	optarg = NULL;

	/* Dash-dash check */
	if(argv[argv_index] && !strcmp(argv[argv_index], "--"))
	{
		dashdash = 1;
		increment_index();
	}

	/* If we're at the end of argv, that's it. */
	if(argv[argv_index] == NULL)
	{
		c = -1;
	}
	/* Are we looking at a string? Single dash is also a string */
	else if(dashdash || argv[argv_index][0] != '-' || !strcmp(argv[argv_index], "-"))
	{
		/* If we want a string... */
		if(optstr[0] == '-')
		{
			c = 1;
			optarg = argv[argv_index];
			increment_index();
		}
		/* If we really don't want it (we're in POSIX mode), we're done */
		else if(optstr[0] == '+' || getenv("POSIXLY_CORRECT"))
		{
			c = -1;

			/* Everything else is a non-opt argument */
			nonopt = argc - argv_index;
		}
		/* If we mildly don't want it, then move it back */
		else
		{
			if(!permute_argv_once()) goto getopt_top;
			else c = -1;
		}
	}
	/* Otherwise we're looking at an option */
	else
	{
		char* opt_ptr = NULL;

		/* Grab the option */
		c = argv[argv_index][opt_offset++];

		/* Is the option in the optstr? */
		if(optstr[0] == '-') opt_ptr = strchr(optstr+1, c);
		else opt_ptr = strchr(optstr, c);
		/* Invalid argument */
		if(!opt_ptr)
		{
			if(opterr)
			{
				fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
			}

			optopt = c;
			c = '?';

			/* Move onto the next option */
			increment_index();
		}
		/* Option takes argument */
		else if(opt_ptr[1] == ':')
		{
			/* ie, -oARGUMENT, -xxxoARGUMENT, etc. */
			if(argv[argv_index][opt_offset] != '\0')
			{
				optarg = &argv[argv_index][opt_offset];
				increment_index();
			}
			/* ie, -o ARGUMENT (only if it's a required argument) */
			else if(opt_ptr[2] != ':')
			{
				/* One of those "you're not expected to understand this" moment */
				if(argv_index2 < argv_index) argv_index2 = argv_index;
				while(argv[++argv_index2] && argv[argv_index2][0] == '-');
				optarg = argv[argv_index2];

				/* Don't cross into the non-option argument list */
				if(argv_index2 + nonopt >= prev_argc) optarg = NULL;

				/* Move onto the next option */
				increment_index();
			}
			else
			{
				/* Move onto the next option */
				increment_index();
			}

			/* In case we got no argument for an option with required argument */
			if(optarg == NULL && opt_ptr[2] != ':')
			{
				optopt = c;
				c = '?';

				if(opterr)
				{
					fprintf(stderr,"%s: option requires an argument -- %c\n",
							argv[0], optopt);
				}
			}
		}
		/* Option does not take argument */
		else
		{
			/* Next argv_index */
			if(argv[argv_index][opt_offset] == '\0')
			{
				increment_index();
			}
		}
	}

	/* Calculate optind */
	if(c == -1)
	{
		optind = argc - nonopt;
	}
	else
	{
		optind = argv_index;
	}

	return c;
}
#endif //!WIN32

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
	
	while (-1 != (o = getopt(argc, (char **)argv, ESQL_BASEARGS))) {
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
		} catch (...) {
			printf("connect to %s failed\n", dbconn.getConnectionDatabaseType().c_str());
			return -1;
		}

		show_dbinfo(dbconn);
		istty = ISTTY(0);
		
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
			if (0 == eso_strncasecmp(s, "go", 2) && (strlen(s) == 2 || isspace(s[2]))) {
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

//=============================================================================

MAIN_IMPL(test_esql) {
	return Main(argc, argv);
}
