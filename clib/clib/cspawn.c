/*
 * Spawning processes.
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com
 *
 * (C) 2006 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <clib-config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <clib.h>

#ifdef C_PLATFORM_UNIX
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#endif

#ifdef WIN32
#include <io.h>
#include <winsock2.h>
#define open _open
#define close _close
#define read _read
#define write _write
/* windows pipe api details:
 * http://msdn2.microsoft.com/en-us/library/edze9h7e(VS.80).aspx */
#define pipe(x) _pipe(x, 256, 0)
#endif

#define set_error(msg, ...)                                                    \
    do {                                                                       \
        if (error != NULL)                                                     \
            *error = c_error_new(C_SPAWN_ERROR, 1, msg, __VA_ARGS__);          \
    } while (0)
#define set_error_cond(cond, msg, ...)                                         \
    do {                                                                       \
        if ((cond) && error != NULL)                                           \
            *error = c_error_new(C_SPAWN_ERROR, 1, msg, __VA_ARGS__);          \
    } while (0)
#define set_error_status(status, msg, ...)                                     \
    do {                                                                       \
        if (error != NULL)                                                     \
            *error = c_error_new(C_SPAWN_ERROR, status, msg, __VA_ARGS__);     \
    } while (0)
#define NO_INTR(var, cmd)                                                      \
    do {                                                                       \
        (var) = (cmd);                                                         \
    } while ((var) == -1 && errno == EINTR)
#define CLOSE_PIPE(p)                                                          \
    do {                                                                       \
        close(p[0]);                                                           \
        close(p[1]);                                                           \
    } while (0)

#if defined(__APPLE__) && !defined(__arm__)
/* Apple defines this in crt_externs.h but doesn't provide that header for
 * arm-apple-darwin9.  We'll manually define the symbol on Apple as it does
 * in fact exist on all implementations (so far)
 */
char ***_NSGetEnviron();
#define environ (*_NSGetEnviron())
#elif defined(_MSC_VER)
/* MS defines this in stdlib.h */
#else
extern char **environ;
#endif

c_quark_t
c_spawn_error_get_quark(void)
{
    return c_quark_from_static_string("g-spawn-error-quark");
}

#ifndef WIN32
static int
safe_read(int fd, char *buffer, int count, c_error_t **error)
{
    int res;

    NO_INTR(res, read(fd, buffer, count));
    set_error_cond(res == -1, "%s", "Error reading from pipe.");
    return res;
}

static int
read_pipes(
    int outfd, char **out_str, int errfd, char **err_str, c_error_t **error)
{
    fd_set rfds;
    int res;
    bool out_closed;
    bool err_closed;
    c_string_t *out = NULL;
    c_string_t *err = NULL;
    char *buffer = NULL;
    int nread;

    out_closed = (outfd < 0);
    err_closed = (errfd < 0);
    if (out_str) {
        *out_str = NULL;
        out = c_string_new("");
    }

    if (err_str) {
        *err_str = NULL;
        err = c_string_new("");
    }

    do {
        if (out_closed && err_closed)
            break;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4389)
#endif

        FD_ZERO(&rfds);
        if (!out_closed && outfd >= 0)
            FD_SET(outfd, &rfds);
        if (!err_closed && errfd >= 0)
            FD_SET(errfd, &rfds);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

        res = select(MAX(outfd, errfd) + 1, &rfds, NULL, NULL, NULL);
        if (res > 0) {
            if (buffer == NULL)
                buffer = c_malloc(1024);
            if (!out_closed && FD_ISSET(outfd, &rfds)) {
                nread = safe_read(outfd, buffer, 1024, error);
                if (nread < 0) {
                    close(errfd);
                    close(outfd);
                    return -1;
                }
                c_string_append_len(out, buffer, nread);
                if (nread <= 0) {
                    out_closed = true;
                    close(outfd);
                }
            }

            if (!err_closed && FD_ISSET(errfd, &rfds)) {
                nread = safe_read(errfd, buffer, 1024, error);
                if (nread < 0) {
                    close(errfd);
                    close(outfd);
                    return -1;
                }
                c_string_append_len(err, buffer, nread);
                if (nread <= 0) {
                    err_closed = true;
                    close(errfd);
                }
            }
        }
    } while (res > 0 || (res == -1 && errno == EINTR));

    c_free(buffer);
    if (out_str)
        *out_str = c_string_free(out, false);

    if (err_str)
        *err_str = c_string_free(err, false);

    return 0;
}

static bool
create_pipe(int *fds, c_error_t **error)
{
    if (pipe(fds) == -1) {
        set_error("%s", "Error creating pipe.");
        return false;
    }
    return true;
}
#endif /* WIN32 */

static int
write_all(int fd, const void *vbuf, size_t n)
{
    const char *buf = (const char *)vbuf;
    size_t nwritten = 0;
    int w;

    do {
        do {
            w = write(fd, buf + nwritten, n - nwritten);
        } while (w == -1 && errno == EINTR);

        if (w == -1)
            return -1;

        nwritten += w;
    } while (nwritten < n);

    return nwritten;
}

bool
c_spawn_sync(const char *working_dir,
             char **argv,
             char **envp,
             c_spawn_flags_t flags,
             c_spawn_child_setup_func_t child_setup,
             void *user_data,
             char **standard_output,
             char **standard_error,
             int *exit_status,
             c_error_t **error)
{
#ifdef WIN32
#else
    pid_t pid;
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    int status;
    int res;

    if (standard_output && !create_pipe(stdout_pipe, error))
        return false;

    if (standard_error && !create_pipe(stderr_pipe, error)) {
        if (standard_output) {
            CLOSE_PIPE(stdout_pipe);
        }
        return false;
    }

    pid = fork();
    if (pid == 0) {
        int i;

        if (standard_output) {
            close(stdout_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
        } else if (flags & C_SPAWN_STDOUT_TO_DEV_NULL) {
            int nullfd = open("/dev/null", O_WRONLY);
            dup2(nullfd, STDOUT_FILENO);
        }

        if (standard_error) {
            close(stderr_pipe[0]);
            dup2(stderr_pipe[1], STDERR_FILENO);
        } else if (flags & C_SPAWN_STDERR_TO_DEV_NULL) {
            int nullfd = open("/dev/null", O_WRONLY);
            dup2(nullfd, STDERR_FILENO);
        }

        if (!(flags & C_SPAWN_CHILD_INHERITS_STDIN)) {
            int nullfd = open("/dev/null", O_RDONLY);
            dup2(nullfd, STDIN_FILENO);
        }

        if (!(flags & C_SPAWN_LEAVE_DESCRIPTORS_OPEN))
            for (i = getdtablesize() - 1; i >= 3; i--)
                close(i);

        if (working_dir) {
            if (chdir(working_dir) == -1) {
                fprintf(stderr, "Failed to change directory to %s: %s",
                        working_dir, strerror(errno));
                exit(1);
            }
        }

        if (flags & C_SPAWN_SEARCH_PATH && !c_path_is_absolute(argv[0])) {
            char *arg0;

            arg0 = c_find_program_in_path(argv[0]);
            if (arg0 == NULL) {
                exit(1);
            }
            // c_free (argv [0]);
            argv[0] = arg0;
        }

        if (child_setup)
            child_setup(user_data);

        execv(argv[0], argv);
        exit(1); /* TODO: What now? */
    }

    if (standard_output)
        close(stdout_pipe[1]);

    if (standard_error)
        close(stderr_pipe[1]);

    if (standard_output || standard_error) {
        res = read_pipes(stdout_pipe[0],
                         standard_output,
                         stderr_pipe[0],
                         standard_error,
                         error);
        if (res) {
            waitpid(pid, &status, WNOHANG); /* avoid zombie */
            return false;
        }
    }

    NO_INTR(res, waitpid(pid, &status, 0));

    /* TODO: What if error? */
    if (WIFEXITED(status) && exit_status) {
        *exit_status = WEXITSTATUS(status);
    }
#endif
    return true;
}

bool
c_spawn_command_line_sync(const char *command_line,
                          char **standard_output,
                          char **standard_error,
                          int *exit_status,
                          c_error_t **error)
{
    char **argv;
    int argc;
    bool status;

    if (!c_shell_parse_argv(command_line, &argc, &argv, error))
        return false;

    status = c_spawn_sync(NULL, /* inherit working dir */
                          argv,
                          NULL, /* env */
                          C_SPAWN_SEARCH_PATH,
                          NULL, /* child setup func */
                          NULL, /* user data */
                          standard_output,
                          standard_error,
                          exit_status,
                          error);
    c_strfreev(argv);

    return status;
}

/*
 * This is the only use we have in mono/metadata
   !c_spawn_async_with_pipes (NULL, (char**)addr_argv, NULL, C_SPAWN_SEARCH_PATH,
   NULL, NULL, &child_pid, &ch_in, &ch_out, NULL, NULL)
 */
bool
c_spawn_async_with_pipes(const char *working_directory,
                         char **argv,
                         char **envp,
                         c_spawn_flags_t flags,
                         c_spawn_child_setup_func_t child_setup,
                         void *user_data,
                         c_pid_t *child_pid,
                         int *standard_input,
                         int *standard_output,
                         int *standard_error,
                         c_error_t **error)
{
#ifdef WIN32
#else
    pid_t pid;
    int info_pipe[2];
    int in_pipe[2] = { -1, -1 };
    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };
    int status;

    c_return_val_if_fail(argv != NULL, false); /* Only mandatory arg */

    if (!create_pipe(info_pipe, error))
        return false;

    if (standard_output && !create_pipe(out_pipe, error)) {
        CLOSE_PIPE(info_pipe);
        return false;
    }

    if (standard_error && !create_pipe(err_pipe, error)) {
        CLOSE_PIPE(info_pipe);
        CLOSE_PIPE(out_pipe);
        return false;
    }

    if (standard_input && !create_pipe(in_pipe, error)) {
        CLOSE_PIPE(info_pipe);
        CLOSE_PIPE(out_pipe);
        CLOSE_PIPE(err_pipe);
        return false;
    }

    pid = fork();
    if (pid == -1) {
        CLOSE_PIPE(info_pipe);
        CLOSE_PIPE(out_pipe);
        CLOSE_PIPE(err_pipe);
        CLOSE_PIPE(in_pipe);
        set_error("%s", "Error in fork ()");
        return false;
    }

    if (pid == 0) {
        /* No zombie left behind */
        if ((flags & C_SPAWN_DO_NOT_REAP_CHILD) == 0) {
            pid = fork();
        }

        if (pid != 0) {
            exit(pid == -1 ? 1 : 0);
        } else {
            int i;
            int fd;
            char *arg0;
            char **actual_args;
            int unused;

            close(info_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(err_pipe[0]);

            /* when exec* succeeds, we want to close this fd, which will return
             * a 0 read on the parent. We're not supposed to keep it open
             * forever.
             * If exec fails, we still can write the error to it before closing.
             */
            fcntl(info_pipe[1], F_SETFD, FD_CLOEXEC);

            if ((flags & C_SPAWN_DO_NOT_REAP_CHILD) == 0) {
                pid = getpid();
                NO_INTR(unused, write_all(info_pipe[1], &pid, sizeof(pid_t)));
            }

            if (working_directory && chdir(working_directory) == -1) {
                int err = errno;
                NO_INTR(unused, write_all(info_pipe[1], &err, sizeof(int)));
                exit(0);
            }

            if (standard_output) {
                dup2(out_pipe[1], STDOUT_FILENO);
            } else if ((flags & C_SPAWN_STDOUT_TO_DEV_NULL) != 0) {
                fd = open("/dev/null", O_WRONLY);
                dup2(fd, STDOUT_FILENO);
            }

            if (standard_error) {
                dup2(err_pipe[1], STDERR_FILENO);
            } else if ((flags & C_SPAWN_STDERR_TO_DEV_NULL) != 0) {
                fd = open("/dev/null", O_WRONLY);
                dup2(fd, STDERR_FILENO);
            }

            if (standard_input) {
                dup2(in_pipe[0], STDIN_FILENO);
            } else if ((flags & C_SPAWN_CHILD_INHERITS_STDIN) == 0) {
                fd = open("/dev/null", O_RDONLY);
                dup2(fd, STDIN_FILENO);
            }

            if ((flags & C_SPAWN_LEAVE_DESCRIPTORS_OPEN) != 0) {
                for (i = getdtablesize() - 1; i >= 3; i--)
                    close(i);
            }

            actual_args =
                ((flags & C_SPAWN_FILE_AND_ARGV_ZERO) == 0) ? argv : argv + 1;
            if (envp == NULL)
                envp = environ;

            if (child_setup)
                child_setup(user_data);

            arg0 = argv[0];
            if (!c_path_is_absolute(arg0) ||
                (flags & C_SPAWN_SEARCH_PATH) != 0) {
                arg0 = c_find_program_in_path(argv[0]);
                if (arg0 == NULL) {
                    int err = ENOENT;
                    write_all(info_pipe[1], &err, sizeof(int));
                    exit(0);
                }
            }

            execve(arg0, actual_args, envp);
            write_all(info_pipe[1], &errno, sizeof(int));
            exit(0);
        }
    } else if ((flags & C_SPAWN_DO_NOT_REAP_CHILD) == 0) {
        int w;
        /* Wait for the first child if two are created */
        NO_INTR(w, waitpid(pid, &status, 0));
        if (status == 1 || w == -1) {
            CLOSE_PIPE(info_pipe);
            CLOSE_PIPE(out_pipe);
            CLOSE_PIPE(err_pipe);
            CLOSE_PIPE(in_pipe);
            set_error("Error in fork (): %d", status);
            return false;
        }
    }
    close(info_pipe[1]);
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);

    if ((flags & C_SPAWN_DO_NOT_REAP_CHILD) == 0) {
        int x;
        NO_INTR(x, read(info_pipe[0], &pid, sizeof(pid_t))); /* if we read <
                                                                sizeof
                                                                (pid_t)... */
    }

    if (child_pid) {
        *child_pid = pid;
    }

    if (read(info_pipe[0], &status, sizeof(int)) != 0) {
        close(info_pipe[0]);
        close(in_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[1]);
        set_error_status(
            status, "Error in exec (%d -> %s)", status, strerror(status));
        return false;
    }

    close(info_pipe[0]);
    if (standard_input)
        *standard_input = in_pipe[1];
    if (standard_output)
        *standard_output = out_pipe[0];
    if (standard_error)
        *standard_error = err_pipe[0];
#endif
    return true;
}
