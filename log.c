/*
Copyright (c) 2003-2006 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "polipo.h"

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

//The amount of logging is controlled by the variable logLevel
static int logLevel = LOGGING_DEFAULT;
//The Logging location where these messages go is controlled by the configuration variables logFile and logSyslog
//If logSyslog is true, error messages go to the system log facility given by logFacility
//If logSyslog is false and logFile is empty, 
//messages go to the error output of the process (normally the terminal).
static int logSyslog = 0;
static AtomPtr logFile = NULL;
//If logFile is set, it is the name of a file where all output will accumulate.
static FILE *logF;
static int logFilePermissions = 0640;

//Keeping extensive logs on your users browsing habits is 
//probably a serere violation of their privacy. 
//If the variable scrubLogs is set, 
//then Polipo will scrub most, if not all, private information from its logs.
int scrubLogs = 0;

#ifdef HAVE_SYSLOG

static AtomPtr logFacility = NULL;
static int facility;
#endif

#define STR(x) XSTR(x)
#define XSTR(x) #x

static void initSyslog(void);

#ifdef HAVE_SYSLOG
static char *syslogBuf;
static int syslogBufSize;
static int syslogBufLength;

static int translateFacility(AtomPtr facility);
static int translatePriority(int type);
static void accumulateSyslogV(int type, const char *f, va_list args);
static void accumulateSyslogN(int type, const char *s, int len);
#endif

void
preinitLog()
{
    CONFIG_VARIABLE_SETTABLE(logLevel, CONFIG_HEX, configIntSetter,
                             "Logging level (max = " STR(LOGGING_MAX) ").");
    CONFIG_VARIABLE(logFile, CONFIG_ATOM, "Log file (stderr if empty and logSyslog is unset, /var/log/polipo if empty and daemonise is true).");
    CONFIG_VARIABLE(logFilePermissions, CONFIG_OCTAL,
                    "Access rights of the logFile.");
    CONFIG_VARIABLE_SETTABLE(scrubLogs, CONFIG_BOOLEAN, configIntSetter,
                             "If true, don't include URLs in logs.");

#ifdef HAVE_SYSLOG
    CONFIG_VARIABLE(logSyslog, CONFIG_BOOLEAN, "Log to syslog.");
    CONFIG_VARIABLE(logFacility, CONFIG_ATOM, "Syslog facility to use.");
	//otherwise. The variable logSyslog defaults to false, and logFacility defaults to ‘user’.
    logFacility = internAtom("user");
#endif

    logF = stderr;
}

int
loggingToStderr(void) {
    return(logF == stderr);
}

//logFile一定存在
static FILE *
openLogFile(void)
{
    int fd;
    FILE *f;
	//包含在头文件：sys/types.h，sys/stat.h，fcntl.h中
	//int open(const char * pathname, int flags, mode_t mode);
	//If logFile is set, 
	//then the variable logFilePermissions controls the Unix permissions 
	//with which the log file will be created if it doesn’t exist.
	//It defaults to 0640.
    fd = open(logFile->string, O_WRONLY | O_CREAT | O_APPEND,
              logFilePermissions);
    if(fd < 0) //打开文件失败
        return NULL;
	//打开文件成功 返回fd=0
    f = fdopen(fd, "a");
    if(f == NULL) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return NULL;
    }

    setvbuf(f, NULL, _IOLBF, 0); //s+ to search
    return f;
}

void
initLog(void)
{	//The variable logFile defaults to empty if daemonise is false, and to ‘/var/log/polipo’
	//otherwise. The variable logSyslog defaults to false, and logFacility defaults to ‘user’.
    if(daemonise && logFile == NULL && !logSyslog) //2^3 = 8
        logFile = internAtom("/var/log/polipo");
    // daemonise: true; logFile:NULL; logSyslog: true
    // daemonise: true; logFile:NULL; logSyslog: false  logFile
    // daemonise: true; logFile:!NULL; logSyslog: true
    // daemonise: true; logFile:!NULL; logSyslog: false
    // daemonise: false; logFile:NULL; logSyslog: true
    // daemonise: false; logFile:NULL; logSyslog: false
    // daemonise: false; logFile:!NULL; logSyslog: true
    // daemonise: false; logFile:!NULL; logSyslog: false
    //4种情况
    if(logFile != NULL && logFile->length > 0) {
        FILE *f;
        logFile = expandTilde(logFile);
        f = openLogFile();
        if(f == NULL) {
            do_log_error(L_ERROR, errno, "Couldn't open log file %s",
                         logFile->string);
            exit(1);
        }
        logF = f;
    }
    //4种情况
    if(logSyslog) {
        initSyslog();

        if(logFile == NULL) {
            logF = NULL;
        }
    }
}

#ifdef HAVE_SYSLOG
static void
initSyslog()
{
    if(logSyslog) {
        facility = translateFacility(logFacility);
        closelog();
        openlog("polipo", LOG_PID, facility);

        if(!syslogBuf) {
            syslogBuf = strdup("");
            syslogBufSize = 1;
        }
    }
}

/* Map a user-provided name to a syslog facility.

   This is rendered quite ugly because POSIX hardly defines any, but we
   should allow any the local system knows about. */

static int
translateFacility(AtomPtr facility)
{
    typedef struct
    {
        const char *name;
        int facility;
    } FacilitiesRec;

    /* List of all known valid syslog facilities.

       This list is terminated by a NULL facility name. */

    FacilitiesRec facilities[] = {
        /* These are all the facilities found in glibc 2.5. */
#ifdef LOG_AUTH
        { "auth", LOG_AUTH },
#endif
#ifdef LOG_AUTHPRIV
        { "authpriv", LOG_AUTHPRIV },
#endif
#ifdef LOG_CRON
        { "cron", LOG_CRON },
#endif
#ifdef LOG_DAEMON
        { "daemon", LOG_DAEMON },
#endif
#ifdef LOG_FTP
        { "ftp", LOG_FTP },
#endif
#ifdef LOG_KERN
        { "kern", LOG_KERN },
#endif
#ifdef LOG_LPR
        { "lpr", LOG_LPR },
#endif
#ifdef LOG_MAIL
        { "mail", LOG_MAIL },
#endif
#ifdef LOG_NEWS
        { "news", LOG_NEWS },
#endif
#ifdef LOG_SYSLOG
        { "syslog", LOG_SYSLOG },
#endif
#ifdef LOG_uucp
        { "uucp", LOG_UUCP },
#endif
        /* These are required by POSIX. */
        { "user", LOG_USER },
        { "local0", LOG_LOCAL0 },
        { "local1", LOG_LOCAL1 },
        { "local2", LOG_LOCAL2 },
        { "local3", LOG_LOCAL3 },
        { "local4", LOG_LOCAL4 },
        { "local5", LOG_LOCAL5 },
        { "local6", LOG_LOCAL6 },
        { "local7", LOG_LOCAL7 },
        { NULL, 0 }};

    FacilitiesRec *current;

    /* It would be more fitting to return LOG_DAEMON, but POSIX does not
       guarantee the existence of that facility. */

    if(!facility) {
        return LOG_USER;
    }

    current = facilities;
    while(current->name) {
        if(!strcmp(current->name, atomString(facility))) {
            return current->facility;
        }
        current++;
    }

    /* This will go to stderr because syslog is not yet initialized. */
    do_log(L_ERROR, "Specified logFacility %s nonexistent on this system.",
           atomString(facility));

    return LOG_USER;
}

/* Translate a Polipo error type into a syslog priority. */

static int
translatePriority(int type)
{
    typedef struct
    {
        int type;
        int priority;
    } PrioritiesRec;

    /* The list is terminated with a type of zero. */

    PrioritiesRec priorities[] = {{ L_ERROR, LOG_ERR },
                                  { L_WARN, LOG_WARNING },
                                  { L_INFO, LOG_NOTICE },
                                  { L_FORBIDDEN, LOG_DEBUG },
                                  { L_UNCACHEABLE, LOG_DEBUG },
                                  { L_SUPERSEDED, LOG_DEBUG },
                                  { L_VARY, LOG_DEBUG },
                                  { L_TUNNEL, LOG_NOTICE },
                                  { 0, 0 }};
    PrioritiesRec *current;

    current = priorities;
    while(current->type) {
        if(current->type == type) {
            return current->priority;
        }
        current++;
    }

    return LOG_DEBUG;
}

static int
expandSyslog(int len)
{
    int newsize;
    char *newbuf;

    if(len < 0)
        newsize = syslogBufSize * 2;
    else
        newsize = syslogBufLength + len + 1;

    newbuf = realloc(syslogBuf, newsize);
    if(!newbuf)
        return -1;

    syslogBuf = newbuf;
    syslogBufSize = newsize;
    return 1;
}

static void
maybeFlushSyslog(int type)
{
    char *linefeed;
    while(1) {
        linefeed = memchr(syslogBuf, '\n', syslogBufLength);
        if(linefeed == NULL)
            break;
        *linefeed = '\0';
        syslog(translatePriority(type), "%s", syslogBuf);
        linefeed++;
        syslogBufLength -= (linefeed - syslogBuf);
        if(syslogBufLength > 0)
            memmove(syslogBuf, linefeed, syslogBufLength);
    }
}

static void
accumulateSyslogV(int type, const char *f, va_list args)
{
    int rc;
    va_list args_copy;

 again:
    va_copy(args_copy, args);
    rc = vsnprintf(syslogBuf + syslogBufLength,
                   syslogBufSize - syslogBufLength,
                   f, args_copy);
    va_end(args_copy);

    if(rc < 0 || rc >= syslogBufSize - syslogBufLength) {
        rc = expandSyslog(rc);
        if(rc < 0)
            return;
        goto again;
    }

    syslogBufLength += rc;

    maybeFlushSyslog(type);
}

static void
accumulateSyslogN(int type, const char *s, int len)
{
    while(syslogBufSize - syslogBufLength <= len)
        expandSyslog(len);

    memcpy(syslogBuf + syslogBufLength, s, len);
    syslogBufLength += len;
    syslogBuf[syslogBufLength] = '\0';

    maybeFlushSyslog(type);
}

#else
static void
initSyslog()
{
    return;
}
#endif

/* Flush any messages waiting to be logged. */
void flushLog()
{
    if(logF)
        fflush(logF);

#ifdef HAVE_SYSLOG
    /* There shouldn't really be anything here, but let's be paranoid.
       We can't pick a good value for `type', so just invent one. */
    if(logSyslog && syslogBuf[0] != '\0') {
        accumulateSyslogN(L_INFO, "\n", 1);
    }

    assert(syslogBufLength == 0);
#endif
}

void
reopenLog()
{
    if(logFile && logFile->length > 0) {
        FILE *f;
        f = openLogFile();
        if(f == NULL) {
            do_log_error(L_ERROR, errno, "Couldn't reopen log file %s",
                         logFile->string);
            exit(1);
        }
        fclose(logF);
        logF = f;
    }

    if(logSyslog)
        initSyslog();
}

void
really_do_log(int type, const char *f, ...)
{
    va_list args;

    va_start(args, f);
    if(type & LOGGING_MAX & logLevel)
        really_do_log_v(type, f, args);
    va_end(args);
}

void
really_do_log_v(int type, const char *f, va_list args)
{
    va_list args_copy;

    if(type & LOGGING_MAX & logLevel) {
        if(logF)
        {
            va_copy(args_copy, args);
            vfprintf(logF, f, args_copy);
            va_end(args_copy);
        }
#ifdef HAVE_SYSLOG
        if(logSyslog) {
            va_copy(args_copy, args);
            accumulateSyslogV(type, f, args_copy);
            va_end(args_copy);
        }
#endif
    }
}

void
really_do_log_error(int type, int e, const char *f, ...)
{
    va_list args;
    va_start(args, f);
    if(type & LOGGING_MAX & logLevel)
        really_do_log_error_v(type, e, f, args);
    va_end(args);
}

void
really_do_log_error_v(int type, int e, const char *f, va_list args)
{
    va_list args_copy;

    if((type & LOGGING_MAX & logLevel) != 0) {
        char *es = pstrerror(e);
        if(es == NULL)
            es = "Unknown error";

        if(logF) {
            va_copy(args_copy, args);
            vfprintf(logF, f, args_copy);
            fprintf(logF, ": %s\n", es);
            va_end(args_copy);
        }
#ifdef HAVE_SYSLOG
        if(logSyslog) {
            char msg[256];
            int n = 0;

            va_copy(args_copy, args);
            n = snnvprintf(msg, n, 256, f, args_copy);
            va_end(args_copy);
            n = snnprintf(msg, n, 256, ": ");
            n = snnprint_n(msg, n, 256, es, strlen (es));
            n = snnprintf(msg, n, 256, "\n");
            /* Overflow? Vanishingly unlikely; truncate at 255. */
            if(n < 0 || n > 256) {
                n = 256;
                msg[255] = '\0';
            }
            else
                msg[n] = '\0';

            accumulateSyslogN(type, msg, n);
        }
#endif
    }
}

void
really_do_log_n(int type, const char *s, int n)
{
    if((type & LOGGING_MAX & logLevel) != 0) {
        if(logF) {
            fwrite(s, n, 1, logF);
        }
#ifdef HAVE_SYSLOG
        if(logSyslog)
            accumulateSyslogN(type, s, n);
#endif
    }
}

const char *
scrub(const char *message)
{
    if(scrubLogs)
        return "(scrubbed)";
    else
        return message;
}
