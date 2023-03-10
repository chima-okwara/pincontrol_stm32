#!/usr/bin/env python3

import json, re, sys, xmltodict
from os import path

svdName = sys.argv[1]
svdDir = path.expanduser("~/.platformio/platforms/ststm32/misc/svd")

# parse System View Description file
# see https://www.keil.com/pack/doc/CMSIS/SVD/html/svd_Format_pg.html
with open("%s/%s.svd" % (svdDir, svdName), 'rb') as f:
    parsed = xmltodict.parse(f)
#print(json.dumps(parsed, indent=2))

# see https://stackoverflow.com/questions/4836710
def natsort(s, _nsre=re.compile('([0-9]+)')):
    return [int(text) if text.isdigit() else text.lower()
            for text in _nsre.split(str(s))]

irqs, periphs, rccs, enables = {}, [], [], {}
irqLimit = 0

for p in parsed['device']['peripherals']['peripheral']:
    u = p['name'].upper()
    #g = p.get('groupName', '').upper() or g
    b = int(p['baseAddress'], 0)

    if u == 'NVIC':
        b = 0xE000E100 # fix: sometimes it's defined as 0xE000E000
    periphs.append("constexpr IoReg<0x%04X'%04X> %s;" % (b>>16, b&0xFFFF, u))

    interrupts = p.get('interrupt', [])
    if type(interrupts) is not list:
        interrupts = [interrupts]
    for x in interrupts:
        n, v = x['name'], int(x['value'])
        if '_EXTI' in n: # fix U[S]ART<n>_EXTI<m> in F302
            n = n[:n.index('_EXTI')]
        if n.startswith('DMA_STR'): # DMA[1]_STR<n> in H745
            n = n[:3] + '1' + n[3:]
        irqs[n] = v
        if v >= irqLimit:
            irqLimit = v + 1

    registers = p.get('registers', [])
    if registers:
        for x in registers['register']:
            if type(x) is str:
                continue
            rn = x['name']
            if u == "RCC" and re.match('A[HP]B\d?L?ENR', rn):
                b = int(x.get('addressOffset'), 0)
                rccs.append((rn, b))
                for f in x['fields']['field']:
                    if type(f) is str:
                        continue
                    nn = f['name']
                    if nn.endswith('EN'):
                        nn = "EN_" + nn[:-2]
                        if nn == 'EN_DMA':
                            nn += '1' # fix for F302 and L053
                        bb = int(f['bitOffset'])
                        enables[nn] = (bb, rn)

#print(periphs, file=sys.stderr)

hasScb, hasStk = False, False
for x in periphs:
    hasScb = hasScb or x.endswith(' SCB;')
    hasStk = hasStk or x.endswith(' STK;')
if not hasScb:
    periphs.append("constexpr IoReg<0xE000'ED00> SCB;")
if not hasStk:
    periphs.append("constexpr IoReg<0xE000'E010> STK;")

if svdName == 'STM32L4x2':
    enables['EN_USART3'] = (18, 'APB1ENR1') # missing

perOut = "\n".join(sorted(periphs, key=lambda s: natsort(s[28:])))
irqOut = "\n    ".join(("%-22s = %3s," % (t, irqs[t]) for t in sorted(irqs, key=natsort)))
rccOut = "\n    ".join(("%-8s = 0x%X," % t for t in sorted(rccs)))
enaOut = "\n    ".join(("%-16s = %2d + 8 * %s," % (t, *enables[t]) \
                                            for t in sorted(enables)))

print(f"""\
// This header for {svdName} was generated by svdgen.py - do not edit.

#define STM32{svdName[5:7]} 1

constexpr auto SVDNAME = "{svdName}";

{perOut}

enum struct Irq : uint8_t {{
    {irqOut}
    limit = {irqLimit}
}};

enum : uint16_t {{
    {rccOut}
}};

enum : uint16_t {{
    {enaOut}
}};

// End of generated header.""")
