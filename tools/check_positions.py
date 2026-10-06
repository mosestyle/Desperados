import sys,os,glob,pickle; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from elem import chunks, scan_elements
from move import parse_move
from dvf import parse_dvf
def inside(pt, poly):
    x,y=pt; c=False; n=len(poly)
    for i in range(n):
        x0,y0=poly[i]; x1,y1=poly[(i+1)%n]
        if (y0>y)!=(y1>y) and x < x0+(y-y0)*(x1-x0)/(y1-y0): c=not c
    return c
def walkable(pt, layer):
    for a in layer['areas']:
        if inside(pt,a['outline']) and not any(inside(pt,h) for h in a['holes']): return True
    return False
def anchors(G, idx):
    cache={}
    def get(folder,file,setname):
        k=(folder,file.lower())
        if k not in cache:
            p=[x for x in os.listdir(f'{G}/data/{folder}') if x.lower()==file.lower()+'.dvf'][0]
            cache[k]={s['name'].lower():s for s in parse_dvf(open(f'{G}/data/{folder}/{p}','rb').read())['sets']}
        s=cache[k].get(setname.lower()) or list(cache[k].values())[0]
        return s['records'][0]['anchor'] if s['records'] else s['anchor']
    return get
if __name__=='__main__':
    G,S=sys.argv[1:3]; idx=pickle.load(open(S+'/dvfidx.pkl','rb')); get=anchors(G,idx)
    tot=bad=0
    for f in sorted(glob.glob(G+'/data/levels/level_*.dvd')):
        ch=chunks(open(f,'rb').read()); L=parse_move(ch['MOVE'])['layers']
        miss=[]
        for el in scan_elements(ch['ELEM'],idx):
            if el['kind']!='actor': continue
            ax,ay=get(el['folder'],el['file'],el['set']); foot=(el['x']+ax, el['y']+ay)
            fl=el['posture']; tot+=1
            ok = fl < len(L) and walkable(foot,L[fl])
            okany = any(walkable(foot,l) for l in L)
            if not ok: bad+=1; miss.append((el['set'],foot,fl,'on other layer' if okany else 'nowhere'))
        print(f[-12:], 'misplaced', len(miss), miss[:8])
    print('total actors',tot,'not on walkable ground of their floor',bad)
