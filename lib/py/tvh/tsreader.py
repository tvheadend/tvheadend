from dvb_charset_tables import conv_8859_table


def str2hex(s, n=None):
    r = ''
    i = 0
    for c in s:
        r = r + ('%02X ' % ord(c))
        i = i + 1
        if n is not None and i % n == 0:
            r = r + '\n'
    return r


def dvb_convert_date(data):
    return 0


convert_iso_8859 = [
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1, 11, 12, 13
]


def encode_utf8(c):
    if c <= 0x7f:
        return chr(c)
    elif c <= 0x7ff:
        return unichr((c >> 6 & 0x1f) | 0xc0) + unichr((c & 0x3f) | 0x80)
    return ''


def conv_8859(tnum, data):
    r = u''
    print 'TBL %d' % tnum
    tbl = conv_8859_table[tnum]
    for c in data:
        if ord(c) <= 0x7f:
            r = r + c
        elif ord(c) <= 0x9f:
            r = r + ' '
        else:
            uc = tbl[ord(c) - 0xa0]
            if uc:
                r = r + encode_utf8(uc)
    return r


def dvb_convert_string(data, conv):
    print 'convert(%d)' % conv
    print repr(data)
    if not conv:
        return data
    return conv_8859(conv, data)


class TsPacket(object):
    def __init__(self, data):
        # print 'TS Packet:'
        # print str2hex(data, 16)
        hdr = map(ord, data[:4])
        if hdr[0] != 0x47:
            raise Exception('not valid TS packet')
        self.tx_err = (hdr[1] & 0x80) == 0x80
        self.pl_init = (hdr[1] & 0x40) == 0x40
        self.tx_prio = (hdr[1] & 0x20) == 0x20
        self.pid = ((hdr[1] & 0x1F) << 8) + hdr[2]
        self.tx_scr = (hdr[3] & 0xC0) >> 6
        self.adapt = (hdr[3] & 0x30) >> 4
        self.cont = (hdr[3] & 0x0F)
        self.data = data[4:]


class TsSection(object):
    def __init__(self, pid, data):
        hdr = map(ord, data[:3])
        self.pid = pid
        self.tid = hdr[0]
        self.iscrc = (hdr[1] & 0x80) == 0x80
        self.len = ((hdr[1] & 0x0F) << 8) + hdr[2]
        self.data = data[3:]
        # print 'TS Section:'
        # print hdr
        # print self.tid, self.len, len(data)

    def process(self):
        print 'TS Section:'
        print self.tid, self.len, len(self.data)
        # print str2hex(self.data, 16)
        # print self.data

        # Strip header
        hdr = map(ord, self.data[:11])
        plen = self.len - 11
        data = self.data[11:]
        # print str2hex(data, 16)

        # Process each event
        while plen:
            r = self.process_event(data, plen)
            if r < 0:
                break
            plen = plen - r
            data = data[r:]

    def get_string(self, data, dlen, charset):
        # print 'get_string():'
        # print str2hex(data, 16)
        l = ord(data[0])
        if not l:
            return (None, 0)
        # print l, dlen
        if l + 1 > dlen:
            return (None, -1)
        c = ord(data[1])
        print c
        conv = None
        if c == 0:
            return (None, -1)
        elif c <= 0xb:
            conv = convert_iso_8859[c + 4]
            data = data[1:]
            dlen = dlen - 1
        elif c <= 0xf:
            return (None, -1)
        elif c == 0x10:
            conv = 0
        elif c <= 0x14:
            return (None, -1)
        elif c == 0x15:
            conv = 0
        elif c <= 0x1f:
            return (None, -1)
        else:
            conv = 'default'
        s = dvb_convert_string(data[1:1 + l], conv)
        return (s, l + 1)

    def short_event(self, data, dlen):
        if dlen < 5:
            return None
        lang = data[:3]
        (title, l) = self.get_string(data[3:], dlen - 3, None)
        if l < 0:
            return None
        (sumry, l) = self.get_string(data[3 + l:], dlen - 3 - l, None)
        return (title, sumry)

    def process_event(self, data, elen):
        if elen < 12:
            return -1

        # Get lengths
        hdr = map(ord, data[:12])
        dllen = ((hdr[10] & 0x0F) << 8) + hdr[11]
        data = data[12:]
        elen = elen - 12
        if elen < dllen:
            return -1
        ret = 12 + dllen

        # Header info
        eid = (hdr[0] << 8) + hdr[1]
        start = dvb_convert_date(hdr[2:])

        print 'process event (%d):' % dllen
        print '  EID       :   %d' % eid
        print '  START     : %d' % start

        while dllen > 2:
            dtag = ord(data[0])
            dlen = ord(data[1])
            print 'dtag = 0x%02x, dlen = %d' % (dtag, dlen)

            dllen = dllen - 2
            data = data[2:]
            if dllen < dlen:
                return ret

            if dtag == 0x4d:
                (title, summary) = self.short_event(data, dlen)
                print '  TITLE     : %s' % title
                print '  SUMMARY   : %s' % summary

            dllen = dllen - dlen
            data = data[dlen:]

        return ret


if __name__ == '__main__':
    import sys

    fp = open(sys.argv[1])
    cur = nxt = None
    while True:
        pkt = TsPacket(fp.read(188))

        # Restrict to EIT
        if pkt.pid != 0x12:
            continue

        # Start/End
        if pkt.pl_init:
            ptr = ord(pkt.data[0])
            if ptr == 0x00:
                cur = TsSection(pkt.pid, pkt.data[1:])
            else:
                if cur:
                    cur.data = cur.data + pkt.data[:1 + ptr]
                nxt = TsSection(pkt.pid, pkt.data[1 + ptr:])

        # Middle
        elif cur:
            cur.data = cur.data + pkt.data

        # Complete?
        if cur:
            if len(cur.data) >= cur.len:
                print 'Process Section:'
                # try:
                cur.process()
                # except: pass
                cur = None
                print
                sys.exit(0)
            else:
                print 'waiting for %d bytes' % (cur.len - len(cur.data))

        # Next
        if nxt:
            cur = nxt
