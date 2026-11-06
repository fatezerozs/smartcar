"""Independent PC verification. Reads local files only; no serial or SD API."""
from pathlib import Path
import argparse, hashlib, json, struct, zlib
EXPECTED={"S0000000.BIN":65536,"S0000001.BIN":16777216,"S0000002.BIN":65536,"P0000000.BIN":16777216,"P0000001.BIN":16777216}
def verify_file(path,expected_size=None):
    path=Path(path);kind=path.name[0];chunk=4096 if kind=='S' else 65536
    assert kind in 'SP','Unknown format'
    expected_size=EXPECTED[path.name] if expected_size is None else expected_size
    errors=[];sha=hashlib.sha256();blocks=0;tag=None
    before=path.stat()
    if before.st_size!=expected_size:errors.append(f'size {before.st_size} != {expected_size}')
    if before.st_size%chunk:errors.append('partial block')
    perf_pattern=bytes(((i*37)^(i>>8)^0xA5)&255 for i in range(12,chunk-4)) if kind=='P' else None
    with path.open('rb') as f:
        while data:=f.read(chunk):
            sha.update(data)
            if len(data)!=chunk:break
            magic,seq,current_tag=struct.unpack_from('<III',data)
            if tag is None:tag=current_tag
            reasons=[]
            if magic!=(0x53444447 if kind=='S' else 0x50455246):reasons.append('magic')
            if seq!=blocks:reasons.append('sequence')
            if current_tag!=tag:reasons.append('tag')
            stored=struct.unpack_from('<I',data,chunk-4)[0]
            if zlib.crc32(data[:-4])!=stored:reasons.append('crc32')
            expected=perf_pattern if kind=='P' else bytes(((i*33)^blocks^current_tag^(blocks>>8))&255 for i in range(12,chunk-4))
            if data[12:-4]!=expected:reasons.append('pattern')
            if reasons and len(errors)<64:errors.append({'block':blocks,'errors':reasons})
            blocks+=1
    after=path.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):errors.append('file changed during verification')
    return {'name':path.name,'passed':not errors,'size':before.st_size,'expected_size':expected_size,'block_bytes':chunk,'blocks':blocks,'tag':tag,'sha256':sha.hexdigest(),'errors':errors}
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);args=ap.parse_args()
    results=[]
    for name,size in EXPECTED.items():
        f=args.directory/name
        result=verify_file(f,size) if f.is_file() else {'name':name,'passed':False,'errors':['missing']}
        results.append(result);print(json.dumps(result,ensure_ascii=False),flush=True)
    report={'passed':all(x['passed'] for x in results),'method':'PC Python struct + zlib CRC32 + independent pattern derivation','files':results,'power_cycle':'user confirmed; see acquisition record','not_pc_reader_independent_disk_path':True}
    out=args.directory/'pc-verification.json'
    with out.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
    raise SystemExit(0 if report['passed'] else 1)
if __name__=='__main__':main()
