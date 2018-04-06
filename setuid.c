#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <libgen.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#define PROGRAM "setuid"
#define VERSION "v0.1a"
#define COPYRIGHT "Copyright (©) 2018 CJ Oster (cjo@redhat.com). All " \
	"rights reserved."
#define LICENSE "This program is licensed under the GNU General Pulic\n" \
	"License, version 3 (GPLv3). More information can be found at\n" \
	"https://www.gnu.org/license/gpl.html"

#define DEFAULT_SHELL "/bin/sh"
#define DEFAULT_SHELL_ARGS "--norc --noprofile"
#define DEFAULT_GID 99
#define SHELL_DELIM " \t\f\n\r\v"

extern char **environ;

struct _cleanup_frame {
	void (*func)(void *);
	void *arg;
	int run;
};

#define cleanup_push(routine, argument) do { struct _cleanup_frame \
	_clframe __attribute__ ((__cleanup__ (_cleanup))) = \
	{ .func = (void *)(void *)(routine), .arg = (argument), .run = 1 }

#define cleanup_pop(execute) _clframe.run = execute; } while(0)

static void _cleanup( struct _cleanup_frame * );
static char *copy_string( const char * );
static void usage( const char * );
static void version( const char * );
static void license (const char * );
static void copyright (const char * );
static void dealloc( void ** );
static inline int longopt( const char *, int, char **, int, char *, int *,
	int, int, char **, const char *, ... );
static char *trim(char *);
static void freeargs( char *** );
static size_t arglen( char ** );
static int append( char ***args, const char *str );
//extern int execvpe(const char *file, char *const argv[],
//	char *const envp[]);

static void _cleanup( struct _cleanup_frame *frame )
{
	if( frame && frame->run && frame->func )
		frame->func(frame->arg);
}

static char *copy_string( const char *in )
{
	char *out;

	if( !in )
		return NULL;

	if( (out = malloc(strlen(in)+1)) == NULL ) {
		fprintf(stderr,"Could not allocate memory.\n");
		exit(1);
	}
	strcpy(out, in);

	return out;
}

static void usage( const char *me )
{
	fprintf(stderr,"Usage: %s ( [ -h ] | [ -V ] | [ -C ] | [ -L ] ) | ( [ -c command ] [ -s shell ] [ -a args ] [ -g group ] [ -e ] [ -u ] ) user\n"
			"\t-h:  This help message\n"
			"\t-V:  Print version information and exit\n"
			"\t-C:  Print copyright information and exit\n"
			"\t-L:  Print license information and exit\n"
			"\t-c:  Pass \"-c command\" to shell\n"
			"\t-s:  Use \"shell\" instead of calling user's shell. Leaving out shell will use called-user's shell if exists, or " DEFAULT_SHELL " if it does not.\n"
			"\t-a:  Pass options to shell instead of \"" DEFAULT_SHELL_ARGS "\".\n"
			"\t-u:  Userid to become. The -u is optional.\n"
			"\t-g:  Use group instead of users group or nobody if uid is not in /etc/passwd.\n"
			"\t-e:  Copy environment to new process.\n",
			me && strlen(me) ? me : PROGRAM );
}

static void version( const char *me )
{
	fprintf(stderr, "%s: Version %s\n", me && strlen(me) ? me : PROGRAM, VERSION);
}

static void license (const char *me )
{
	fprintf(stderr, "%s: %s\n", me && strlen(me) ? me : PROGRAM, LICENSE);
}

static void copyright (const char *me )
{
	fprintf(stderr, "%s: %s\n", me && strlen(me) ? me : PROGRAM, COPYRIGHT);
}

static void dealloc( void **in )
{
	if( in && *in ) {
		free(*in);
		*in = NULL;
	}
}

static inline int longopt( const char *prog, int argc, char **argv, int opt, char *optarg, int *optind, int optional, int dashok, char **dest, const char *fmt, ... )
{
	char *thearg = NULL;

	if( optarg )
		thearg = optarg;
	else if( optind && *optind < argc && argv[*optind] && (argv[*optind][0] != '-' || dashok) ) {
		thearg = argv[*optind];
		(*optind)++;
	}

	if( !thearg ) {
		if( !optional ) {
			if( !fmt )
				fprintf(stderr,"%s: option requires an argument -- '%c'\n", prog ? prog : PROGRAM, opt);
			else {
				va_list ap;
				va_start(ap, fmt);
				vfprintf(stderr, fmt, ap);
				va_end(ap);
			}
			return 1;
		}
		else {
			if( dest )
				*dest = NULL;
		}
	} else if( dest ) {
		*dest = copy_string(thearg);
		if( ! *dest )
			return -1;
	}
	return 0;
}

static char *trim( char *in )
{
	size_t i, len;

	if( !in )
		return NULL;

	len = strlen(in);
	for( i = len - 1; i > 0 && isspace(in[i]); i--);
	if( (int)i < len - 1 )
		in[i+1] = '\0';
	len = i + 1;
	for( i = 0; i < len && isspace(in[i]); i++ );
	return &in[i];
}

static void freeargs( char ***in )
{
	
	size_t i;

	if( !in || !*in )
		return;

	for( i = 0; (*in)[i]; i++ ) {
		free((*in)[i]);
		(*in)[i] = NULL;
	}
	free(*in);
	*in = NULL;
}

static size_t arglen( char **args )
{
	size_t i;

	if( !args )
		return 0;

	for( i = 0; args[i]; i++ );

	return i;
}

static int append( char ***args, const char *str )
{
	char **a;
	size_t len;

	if( !args || !str )
		return 1;

	if( !*args ) {
		if( (*args = malloc(sizeof(char *) * 2)) == NULL ) {
			fprintf(stderr, "Could not allocate memory.\n");
			return -1;
		}
		(*args)[1] = NULL;
		len = 1;
	} else {
		len = arglen( *args ) + 1;

		if( (a = realloc(*args, sizeof(char *) * (len + 1))) == NULL ) {
			fprintf(stderr, "Could not allocate memory.\n");
			return -1;
		}
		if( a != *args )
			*args = a;
	}
	(*args)[len] = NULL;

	if( ((*args)[len - 1] = copy_string(str)) == NULL )
		return -1;
	
	return 0;
}

int main( int argc, char **argv, char **envp )
{
	uid_t uid = -1;
	gid_t gid = -1;
	char *prog, *shell, *shell_args, *shell_cmd, *tmpstr, *target_user;
	char **args = NULL, *a;
	struct passwd *pwd;
	struct group *gr;
	int opt, argerr = 0, called_shell = 0;
	int printver = 0, printhelp = 0, printcopy = 0, printlicense = 0;
	int copy_environ = 0;

	if( argc <= 0 || !argv ) {
		fprintf(stderr, "Arguments to main do not make any sense.\n");
		return 1;
	}

	prog = NULL;
	shell = NULL;
	shell_args = NULL;
	shell_cmd = NULL;
	target_user = NULL;
	cleanup_push(dealloc, &shell_cmd);
	cleanup_push(dealloc, &shell_args);
	cleanup_push(dealloc, &shell);
	cleanup_push(dealloc, &prog);
	cleanup_push(dealloc, &target_user);
	cleanup_push(freeargs, &args);

	if( argv[0] && strlen(argv[0]) > 0 ) {
		tmpstr = NULL;
		cleanup_push(dealloc,&tmpstr);
		tmpstr = copy_string(argv[0]);
		if( tmpstr ) {
			prog = copy_string(basename(tmpstr));
		}
		cleanup_pop(1);
	}

	while( (opt = getopt(argc, argv, ":hVLCec::s::g::u::a::")) != -1 ) {
		switch(opt) {
			case 'V':
				printver = 1;
				break;

			case 'h':
				printhelp = 1;
				break;

			case 'C':
				printcopy = 1;
				break;

			case 'L':
				printlicense = 1;
				break;

			case 'c':
				if( !shell_cmd ) {
					if( longopt(prog, argc, argv, opt, optarg, &optind, 0, 0, &shell_cmd, NULL ) < 0 )
						return 1;
					if( !shell_cmd )
						argerr = 1;
				} else {
					fprintf(stderr,"Shell command can only be specified once.\n");
					argerr = 1;
				}
				break;

			case 's':
				if( !shell && !called_shell ) {
					if( longopt(prog, argc, argv, opt, optarg, &optind, 1, 0, &shell, NULL) < 0 )
						return 1;
					if( !shell )
						called_shell = 1;
				} else {
					fprintf(stderr, "Shell can only be specified once.\n");
					argerr = 1;
				}
				break;

			case 'g':
				if( gid == -1 ) {
					tmpstr = NULL;
					cleanup_push(dealloc, &tmpstr);
					gr = NULL;
					if( longopt(prog, argc, argv, opt, optarg, &optind, 0, 0, &tmpstr, NULL ) < 0 )
						return 1;
					if( !tmpstr )
						argerr = 1;
					else {
						if( !(sscanf(tmpstr, "%u", &gid) >= 1) && !(gr = getgrnam(tmpstr)) ) {
							fprintf(stderr, "Could not set group to \"%s\"\n", tmpstr);
							argerr = 1;
						}
						if( gr )
							gid = gr->gr_gid;
					}
					cleanup_pop(1);
				} else {
					fprintf(stderr, "Group can only be specified once.\n");
					argerr = 1;
				}
				break;

			case 'a':
				if( !shell_args ) {
					if( longopt(prog, argc, argv, opt, optarg, &optind, 1, 1, &shell_args, NULL ) < 0 )
						return 1;
				} else {
					fprintf(stderr, "Shell argument can only be specified once.\n");
					argerr = 1;
				}


			case 'e':
				copy_environ = 1;
				break;

			case 'u':
				if( !target_user ) {
					if( longopt(prog, argc, argv, opt, optarg, &optind, 0, 0, &target_user, NULL) < 0 )
						return 1;
					if( !target_user )
						argerr = 1;
				} else {
					fprintf(stderr, "User \"%s\" already specified earlier on command line.\n", target_user);
					argerr = 1;
				}

				break;

			case '?':
				fprintf(stderr, "%s: invalid argument -- '%c'\n", prog, optopt);
				argerr = 1;
				break;

			default:
				fprintf(stderr, "%s: BUG BUG option '%c' not implemented.\n", prog, opt);
				return 1;
		}
	}

	if( !argerr ) {
		if( (target_user && optind != argc) || (!target_user && optind < argc - 1) ) {
			fprintf(stderr,"Excess arguments on command line. Refusing to run.\n");
			argerr = 1;
		}

		if( !target_user ) {
			if( argv[optind] ) {
				if( (target_user = copy_string(argv[optind])) == NULL )
					return 1;
			} else {
				fprintf(stderr,"No user specified.\n");
				argerr = 1;
			}
		}
	}

	if( printver || printhelp || printcopy || printlicense || argerr ) {
		if( printver )
			version(prog ? prog : PROGRAM);
		if( printcopy )
			copyright(prog ? prog : PROGRAM);
		if( printlicense )
			license(prog ? prog : PROGRAM);
		if( printhelp || argerr )
			usage(prog ? prog : PROGRAM);
		if( printver || printhelp || printcopy || printlicense )
			return 0;
		return 1;
	}

	pwd = NULL;
	if( sscanf(target_user, "%u", &uid) < 1 || called_shell ) {
		if( uid != -1 )
			pwd = getpwuid(uid);
		else 
			pwd = getpwnam(target_user);

		if( pwd ) {
			if( gid == -1 )
				gid = pwd->pw_gid;
			if( called_shell && pwd->pw_shell && strlen(pwd->pw_shell) > 0 )
				shell = copy_string(trim(pwd->pw_shell));
			if( uid == -1 )
				uid = pwd->pw_uid;
		}
	}
	if( uid == -1 ) {
		fprintf(stderr, "User \"%s\" does not exist.\n", target_user);
		return 1;
	}

	if( gid == -1 ) {
		if( (gr = getgrnam("nobody")) )
			gid = gr->gr_gid;
		else
			gid = DEFAULT_GID;
	}

	if( called_shell && !shell ) {
		fprintf(stderr, "Could not get shell information for user \"%s\".\n", pwd && pwd->pw_name && strlen(pwd->pw_name) > 0 ? pwd->pw_name : target_user);
		return 1;
	}

	if( !shell_args && !(shell_args = copy_string(DEFAULT_SHELL_ARGS)) )
		return 1;

	if( !shell ) {
		if( (pwd = getpwuid(getuid())) && pwd->pw_shell && strlen(pwd->pw_shell) > 0 ) {
			if( !(shell = copy_string(pwd->pw_shell)) )
				return 1;
		} else {
			if( !(shell = copy_string(DEFAULT_SHELL)) )
				return 1;
		}
	}

	if( shell_cmd )
		printf("Shell command: \"%s\"\n", shell_cmd);
	
	if( geteuid() != 0 )
		fprintf(stderr, "Not running as root. Probably will not work. Trying anyway.\n");

	if( setgid(gid) )
		perror("setgid");
	if( setgroups(1,&gid) )
		perror("setgroups");
	if( setuid(uid) ) {
		perror("setuid");
		fprintf(stderr, "Could not set uid to %u. Exiting.\n", uid);
		return 1;
	}

	if( copy_environ )
		environ = envp;
	else
		environ = NULL;

	args = NULL;
	if( append(&args, basename(shell)) < 0 )
		return 1;
	
	if( (a = strtok(shell_args, SHELL_DELIM)) ) {
		if( append(&args, a) < 0 )
			return 1;
		while( (a = strtok(NULL, SHELL_DELIM)) ) {
			if( append(&args, a) < 0 )
				return 1;
		}
	}

	if( shell_cmd ) {
		if( append(&args, "-c") < 0 || append(&args, shell_cmd) < 0 )
			return 1;
	}

	execvpe(shell, args, environ);
	
	fprintf(stderr, "%s: Could not execute \"%s\": %s\n", prog, shell, strerror(errno));
	fflush(stderr);
	cleanup_pop(1);
	cleanup_pop(1);
	cleanup_pop(1);
	cleanup_pop(1);
	cleanup_pop(1);
	cleanup_pop(1);

	return 0;
}
