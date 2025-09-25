/*
 * File locking
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2014 Jaroslav Kysela
 */
#ifndef TVH_LOCK_H
#define TVH_LOCK_H

int file_lock(const char *lfile, int timeout);
int file_unlock(const char *lfile, int _fd);

#endif /* TVH_LOCK_H */
