import sys,os,pickle; sys.path.insert(0,os.path.dirname(__file__))
from dvf import parse_dvf, frame_rgba
from elem import chunks, scan_elements
from desp import load_image_file, rgb565
from PIL import Image
def render(G, lv, idx, out, crop=None):
    w,h,f,raw=load_image_file(f'{G}/data/levels/level_{lv}.dvm')[0]
    bg=rgb565(raw,w,h).convert('RGBA')
    e=chunks(open(f'{G}/data/levels/level_{lv}.dvd','rb').read())['ELEM']
    cache={}
    draws=[]
    for el in scan_elements(e,idx):
        if el['kind'] not in ('actor','object'): continue
        key=(el['folder'],el['file'].lower())
        if key not in cache:
            p=[x for x in os.listdir(f'{G}/data/{el["folder"]}') if x.lower()==el['file'].lower()+'.dvf'][0]
            b=open(f'{G}/data/{el["folder"]}/{p}','rb').read(); cache[key]=(b,parse_dvf(b))
        b,r=cache[key]
        s=[s for s in r['sets'] if s['name'].lower()==el['set'].lower()][0]
        recs=s['records']
        if el['kind']=='actor':
            d=el['dir'] if s['ndirs']==16 else 0
            rec=[x for x in recs if x['anim']==0 and x['dir']==d] or recs
            rec=rec[0]; base=el['y']
        else:
            rec=recs[0]; base=el['y']+el['z']
        fr,dur,step,ex,ey,snd=rec['entries'][0]
        ax,ay=rec['anchor']
        draws.append((base, el['x']-ax+ex, el['y']-ay+ey, b, r['frames'][fr]))
    for base,x,y,b,fr in sorted(draws,key=lambda t:t[0]):
        im=Image.fromarray(frame_rgba(b,fr)); bg.alpha_composite(im,(x,y)) if x>=0 and y>=0 else bg.paste(im,(x,y),im)
    if crop: bg=bg.crop(crop)
    bg.convert('RGB').save(out); return len(draws)
if __name__=='__main__':
    G,S,lv=sys.argv[1:4]
    idx=pickle.load(open(S+'/dvfidx.pkl','rb'))
    print(render(G,lv,idx,f'{S}/lvl{lv}_elems.png'))
