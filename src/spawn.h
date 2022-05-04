/*
 *  Process spawn functions
 *  Copyright (C) 2008 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPAWN_H
#define SPAWN_H

void spawn_info ( const char *fmt, ... );

void spawn_error ( const char *fmt, ... );

int find_exec ( const char *name, char *out, size_t len );

int spawn_parse_args(char ***argv, int argc, const char *cmd, const char **replace);

void spawn_free_args(char **argv);

int spawn_and_give_stdout(const char *prog, char *argv[], char *envp[],
                          int *rd, pid_t *pid, int redir_stderr);

int spawn_with_passthrough(const char *prog, char *argv[], char *envp[],
                           int od, int *wd, pid_t *pid, int redir_stderr);

int spawnv(const char *prog, char *argv[], pid_t *pid, int redir_stdout, int redir_stderr);

int spawn_reap(pid_t pid, char *stxt, size_t stxtlen);

int spawn_kill(pid_t pid, int sig, int timeout);

void spawn_init(void);

void spawn_done(void);

#endif /* SPAWN_H */
