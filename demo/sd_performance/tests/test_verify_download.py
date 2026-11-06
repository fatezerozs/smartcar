from pathlib import Path
import sys,struct,zlib,hashlib,datetime
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_download import verify_file
root=Path(__file__).resolve().parents[1]/'evidence'/('pc-checker-fixtures-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
root.mkdir()
def make(kind):
    size=4096 if kind=='S' else 65536;result=bytearray()
    for seq in range(2):
        b=bytearray(size);struct.pack_into('<III',b,0,0x53444447 if kind=='S' else 0x50455246,seq,0x1234)
        for i in range(12,size-4):b[i]=((i*33)^seq^0x1234^(seq>>8))&255 if kind=='S' else ((i*37)^(i>>8)^0xA5)&255
        struct.pack_into('<I',b,size-4,zlib.crc32(b[:-4]));result+=b
    return result
assert zlib.crc32(b'123456789')==0xCBF43926
cases=[]
for kind in 'SP':cases.append(('good-'+kind,kind,make(kind),True))
b=make('S');b[100]^=1;cases.append(('crc','S',b,False))
for label,index,value in [('sequence',4096+4,0),('tag',4096+8,0x4321),('magic',4096,123)]:
    b=make('S');struct.pack_into('<I',b,index,value);struct.pack_into('<I',b,8192-4,zlib.crc32(b[4096:8192-4]));cases.append((label,'S',b,False))
b=make('P');b[100]^=1;struct.pack_into('<I',b,65536-4,zlib.crc32(b[:65536-4]));cases.append(('pattern-valid-crc','P',b,False))
cases.append(('truncated','S',make('S')[:-1],False));cases.append(('extra-block','S',make('S')+make('S')[:4096],False))
for label,kind,data,expected in cases:
    sub=root/label;sub.mkdir();f=sub/(kind+'0000000.BIN');f.write_bytes(data);before=hashlib.sha256(data).hexdigest()
    result=verify_file(f,8192 if kind=='S' else 131072)
    assert result['passed']==expected,(label,result)
    assert hashlib.sha256(f.read_bytes()).hexdigest()==before
    print('PASS',label,'(read-only fixture unchanged)')
print('ALL 9 PC CHECKER CASES PASS; corruption with recomputed CRC is rejected')
