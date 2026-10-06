import struct,bz2,zlib
import numpy as np
from PIL import Image
def decomp(p):
    if p[:3]==b'BZh': d=bz2.BZ2Decompressor()
    else: d=zlib.decompressobj()
    return d.decompress(p), d.unused_data
def rgb565(buf,w,h,key=None):
    a=np.frombuffer(buf[:w*h*2],dtype='<u2').reshape(h,w)
    r=((a>>11)&31)*255//31; g=((a>>5)&63)*255//63; b=(a&31)*255//31
    rgba=np.dstack([r,g,b,np.full_like(r,255)]).astype(np.uint8)
    if key is not None: rgba[a==key,3]=0
    return Image.fromarray(rgba)
def load_image_file(path):
    """.dvm/.map/.pak: [u16 w][u16 h][u32 fmt][u32 csize][compressed]... (repeats)"""
    b=open(path,'rb').read(); imgs=[]
    while len(b)>=12:
        w,h,fmt,cs=struct.unpack('<HHII',b[:12])
        raw,b=decomp(b[12:])
        imgs.append((w,h,fmt,raw))
    return imgs
