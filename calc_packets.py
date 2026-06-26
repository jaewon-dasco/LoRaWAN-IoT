import struct

values = [
    663.78204345703125,
    0.1,
    0.2,
    -6.6606902285326619e-012,
    -1.3275371358282298e-016,
    5.3609601348955866e-020
]

all_bytes = []
for i, v in enumerate(values):
    b = struct.pack('<f', v)
    hex_bytes = ' '.join('%02X' % x for x in b)
    pkt = [0xF0, 0x86, 0x0C, 0x01, 0x00, 0xEF, 0x1F, 0x03, 0x00, 0x02, i] + list(b)
    checksum = sum(pkt[1:]) & 0xFF
    pkt.append(checksum)
    pkt.append(0xE0)
    print('[%d] %.16e -> float: %s' % (i, v, hex_bytes))
    print('    %s' % ' '.join('%02X' % x for x in pkt))
    all_bytes.extend(pkt)

save = [0xF0, 0x84, 0x0C, 0x01, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x02, 0x14, 0x00, 0x00, 0x00, 0x00]
cs = sum(save[1:]) & 0xFF
save.append(cs)
save.append(0xE0)
print('[SAVE] Parameter Save')
print('    %s' % ' '.join('%02X' % x for x in save))
all_bytes.extend(save)

print('')
print('=== ALL PACKETS ===')
print(' '.join('%02X' % x for x in all_bytes))
