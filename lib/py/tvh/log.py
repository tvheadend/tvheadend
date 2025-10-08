#!/usr/bin/env python
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2012 Adam Sutton <dev@adamsutton.me.uk>

"""
Some very basic logging routines
"""

#
# Enable debug
#
DEBUG_LVL = None


def debug_init(lvl=None):
    global DEBUG_LVL
    DEBUG_LVL = lvl


#
# Output message
#
def out(pre, msg, **dargs):
    import sys
    import datetime, pprint
    now = datetime.datetime.now()
    if 'pretty' in dargs and dargs['pretty']:
        ind = 2
        if 'indent' in dargs: ind = dargs['indent']
        msg = pprint.pformat(msg, indent=ind, width=70)
    out = '%s %-6s: %s\n' % (now.strftime('%F %T'), pre, msg)
    sys.stderr.write(out)


#
# Debug
#
def debug(msg, lvl=1, **dargs):
    if DEBUG_LVL and lvl <= DEBUG_LVL:
        out('DEBUG', msg, **dargs)


#
# Info
#
def info(msg, **dargs):
    out('INFO', msg, **dargs)


#
# Error
#
def error(msg, **dargs):
    out('ERROR', msg, **dargs)
