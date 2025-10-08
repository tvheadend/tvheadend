/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * Process file operations
 */

#ifndef FILE_H
#define FILE_H

#include <sys/types.h>

size_t file_readall ( int fd, char **outbuf ); 

#endif /* FILE_H */
