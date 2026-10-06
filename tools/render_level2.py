import sys,os,pickle; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from dvf import parse_dvf, frame_rgba
from elem import chunks, scan_elements
from mask import parse_masks, decode_bits
from desp import load_image_file, rgb565
from PIL import Image

def line_y(line, x):
    if x <= line[0][0]: return line[0][1]
    for (x0,y0),(x1,y1) in zip(line, line[1:]):
        if x0 <= x <= x1: return y0 + (y1-y0)*(x-x0)/max(1,x1-x0)
    return line[-1][1]

def mask_alpha(mk):
    rows,_=decode_bits(mk); x,y,w,h=mk['rect']
    bits=np.unpackbits(np.frombuffer(b''.join(rows),np.uint8).reshape(h,-1),axis=1)[:,:w]
    return bits.astype(bool)

def render(G, lv, idx, out, use_masks=True, crop=None, outline=False):
    w,h,f,raw=load_image_file(f'{G}/data/levels/level_{lv}.dvm')[0]
    bgimg=rgb565(raw,w,h).convert('RGBA'); bgarr=np.array(bgimg)
    canvas=bgarr.copy()
    ch=chunks(open(f'{G}/data/levels/level_{lv}.dvd','rb').read())
    masks=[m for m in parse_masks(ch['MASK'])['masks']]
    for m in masks: m['alpha']=mask_alpha(m)
    cache={}; draws=[]
    for el in scan_elements(ch['ELEM'],idx):
        if el['kind'] not in ('actor','object'): continue
        key=(el['folder'],el['file'].lower())
        if key not in cache:
            p=[x for x in os.listdir(f'{G}/data/{el["folder"]}') if x.lower()==el['file'].lower()+'.dvf'][0]
            b=open(f'{G}/data/{el["folder"]}/{p}','rb').read(); cache[key]=(b,parse_dvf(b))
        b,r=cache[key]
        s=[s for s in r['sets'] if s['name'].lower()==el['set'].lower()][0]
        if el['kind']=='actor':
            d=el['dir'] if s['ndirs']==16 else 0
            rec=([x for x in s['records'] if x['anim']==0 and x['dir']==d] or s['records'])[0]
            floor=el['posture']; foot=None
        else:
            rec=s['records'][0]; foot=None; floor=0
        fr,dur,step,ex,ey,snd=rec['entries'][0]; ax,ay=rec['anchor']
        img=frame_rgba(b,r['frames'][fr]); x0=el['x']+ex; y0=el['y']+ey      # position = top-left of the anchor box
        if el['kind']=='actor': foot=(el['x']+ax, el['y']+ay)
        else: foot=(x0+img.shape[1]//2, el['y']+el['z'])
        draws.append((foot[1],x0,y0,img,foot,floor))
    for sorty,x0,y0,img,foot,floor in sorted(draws,key=lambda t:t[0]):
        hh,ww=img.shape[:2]
        X0,Y0=max(0,x0),max(0,y0); X1,Y1=min(w,x0+ww),min(h,y0+hh)
        if X1<=X0 or Y1<=Y0: continue
        sub=img[Y0-y0:Y1-y0, X0-x0:X1-x0].astype(np.float32)
        a=sub[...,3:4]/255.0
        canvas[Y0:Y1,X0:X1,:3]=(canvas[Y0:Y1,X0:X1,:3]*(1-a)+sub[...,:3]*a).astype(np.uint8)
        if not use_masks: continue
        for m in masks:
            if m['group']!=floor or not m['line']: continue
            mx,my,mw,mh=m['rect']
            if mx>=X1 or my>=Y1 or mx+mw<=X0 or my+mh<=Y0: continue
            if foot[1] >= line_y(m['line'],foot[0]): continue      # actor in front of this mask
            ix0,iy0,ix1,iy1=max(mx,X0),max(my,Y0),min(mx+mw,X1),min(my+mh,Y1)
            al=m['alpha'][iy0-my:iy1-my, ix0-mx:ix1-mx]
            reg=canvas[iy0:iy1,ix0:ix1]; reg[al]=bgarr[iy0:iy1,ix0:ix1][al]
    im=Image.fromarray(canvas)
    if outline:
        from PIL import ImageDraw
        d=ImageDraw.Draw(im)
        for m in masks:
            if m['group']==0 and m['line']: d.line(m['line'],fill=(255,0,0,255),width=1)
    if crop: im=im.crop(crop)
    im.convert('RGB').save(out)
if __name__=='__main__':
    G,S,lv=sys.argv[1:4]; idx=pickle.load(open(S+'/dvfidx.pkl','rb'))
    crop=tuple(map(int,sys.argv[4].split(','))) if len(sys.argv)>4 else None
    render(G,lv,idx,f'{S}/lvl{lv}_nomask.png',False,crop)
    render(G,lv,idx,f'{S}/lvl{lv}_mask.png',True,crop)
    render(G,lv,idx,f'{S}/lvl{lv}_lines.png',True,crop,outline=True)
