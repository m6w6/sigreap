#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define err(...) dprintf(STDERR_FILENO, __VA_ARGS__)
#define NMAXPIDS 64

static pid_t childpids[NMAXPIDS+1];
static int lastexitcode;

__attribute__((noreturn))
static void die(const char *what) {
	int e = errno;
	perror(what);
	exit(-e);
}

static inline void block() {
	sigset_t all;
	sigfillset(&all);
	if (0 > sigprocmask(SIG_BLOCK, &all, NULL)) {
		die("sigprocmask(SIG_BLOCK)");
	}
}

static inline void unblock() {
	sigset_t all;
	sigfillset(&all);
	if (0 > sigprocmask(SIG_UNBLOCK, &all, NULL)) {
		die("sigprocmask(SIG_UNBLOCK)");
	}
}

static void reap() {
	int wstatus;
	pid_t wpid;

	do {
		wpid = waitpid(-1, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);
		if (0 < wpid) {
			if (WIFSTOPPED(wstatus) || WIFCONTINUED(wstatus)) {
				err(" child: %ld stop/cont\n", (long) wpid);
			} else if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
				lastexitcode = WEXITSTATUS(wstatus);
				err(" child: %ld exit/kill (%d)\n", (long) wpid, lastexitcode);
			}
		}
	} while (0 < wpid);
}

static bool active(const char *procfs) {
	int i = 0, cfd;
	ssize_t siz;
	char children[NMAXPIDS*32] = "";
	char *ptr = children, *end = ptr, *stop;

	block();
	reap();

	cfd = open(procfs, O_RDONLY);
	if (0 > cfd) {
		die("open(procfs)");
	}
	siz = read(cfd, &children, sizeof(children)-1);
	close(cfd);

	if (1 > siz) {
		err("active: false\n");
		unblock();
		return false;
	}

	stop = ptr + siz - 1;
	while (end < stop && i < NMAXPIDS) {
		childpids[i++] = strtoul(ptr, &end, 10);
		ptr = end;
	}
	childpids[i] = 0;

	err("active: true (%d: %s)\n", i, children);
	if (i >= NMAXPIDS) {
		err("active: too many children (>NMAXPIDS)!\n");
	}
	unblock();

	return true;
}

static void handler(int signo) {
	err("signal: %d\n", signo); /* yes, this is not async-signal-safe */
	if (signo != SIGCHLD) {
		for (int i = 0; i < NMAXPIDS && childpids[i]; ++i) {
			if (0 > kill(childpids[i], signo)) {
				perror("kill(childpid, signo)");
			}
		}
	}
}

static void setup(pid_t child) {
	struct sigaction action = {
		.sa_handler = handler,
		.sa_flags = SA_NOCLDSTOP | SA_RESTART
	};

	childpids[0] = child;

	for (int signo = 1; signo < NSIG; ++signo) {
		if (0 > sigaction(signo, &action, NULL)) {
			if (errno != EINVAL) {
				perror("sigaction(*)");
			}
		}
	}

	if (0 > prctl(PR_SET_CHILD_SUBREAPER, 1)) {
		die("prctl(SET_CHILD_SUBREAPER)");
	}
}

static void loop() {
	char procfs[PATH_MAX+1] = "";
	int siz = snprintf(procfs, PATH_MAX,
			"/proc/%1$u/task/%1$u/children", (unsigned) getpid());

	switch (siz) {
	case -1:
	case PATH_MAX:
		die("snprintf(/proc/.../children)");
	}

	while (active(procfs)) {
		pause();
		errno = 0;
	}
}

int main(int argc, char *argv[]) {
	pid_t child;

	if (argc <= 1) {
		err("Usage: %s <program> [<args> ...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	block();

	switch ((child = fork())) {
	case 0:
		unblock();
		execvp(argv[1], &argv[1]); /* fallthrough */
	case -1:
		die("fork/exec");

	default:
		setup(child);
		unblock();
		loop();
		errno = lastexitcode;
		die("  done");
	}
}
