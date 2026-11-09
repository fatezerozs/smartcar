"""RMR1 bounded decoder and nonce handshake. No motion commands."""
import struct
import zlib


class Decoder:
    def __init__(self):
        self.buffer=bytearray();self.good=0;self.bad=0;self.discarded=0
        self.previous=None;self.gaps=0;self.line=bytearray();self.overflow=False

    def feed(self,data):
        self.buffer.extend(data);lines=[]
        while True:
            at=self.buffer.find(b'RMR1')
            if at<0:
                n=max(0,len(self.buffer)-3);self.discarded+=n;del self.buffer[:n];break
            if at:self.discarded+=at;del self.buffer[:at]
            if len(self.buffer)<10:break
            seq,length=struct.unpack_from('<IH',self.buffer,4)
            if not 1<=length<=96:
                self.bad+=1;del self.buffer[0];continue
            size=14+length
            if len(self.buffer)<size:break
            frame=bytes(self.buffer[:size])
            if zlib.crc32(frame[:-4])!=struct.unpack_from('<I',frame,size-4)[0]:
                self.bad+=1;self.line.clear();self.overflow=True;del self.buffer[0];continue
            del self.buffer[:size];self.good+=1
            if self.previous is None or seq!=((self.previous+1)&0xffffffff):
                self.gaps+=self.previous is not None;self.line.clear();self.overflow=True
            self.previous=seq
            for c in frame[10:-4]:
                if c==10:
                    if not self.overflow:
                        try:lines.append(self.line.rstrip(b'\r').decode('ascii'))
                        except UnicodeDecodeError:self.bad+=1
                    self.line.clear();self.overflow=False
                elif not self.overflow:
                    self.line.append(c)
                    if len(self.line)>4096:self.line.clear();self.overflow=True
        return lines


def valid_pong(line,nonce,source):
    return line==f'MOTION PONG nonce={nonce} source={source} c26=1'
