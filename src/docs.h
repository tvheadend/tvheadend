/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Jaroslav Kysela
 *
 * TVheadend
 */

#ifndef __TVH_DOCS_H__
#define __TVH_DOCS_H__

struct tvh_doc_page {
  const char *name;
  const char **strings;
};

extern const struct tvh_doc_page tvh_doc_markdown_pages[];

#endif /* __TVH_DOCS_H__ */
