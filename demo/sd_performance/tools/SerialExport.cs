using System;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.Threading;
using System.Globalization;

public static class SdSerialExport {
    static readonly string[] Names={"P0000000.BIN","P0000001.BIN","S0000000.BIN","S0000001.BIN","S0000002.BIN"};
    static readonly int[] Sizes={16777216,16777216,65536,16777216,65536};
    static readonly uint[] Table=BuildTable();
    static uint[] BuildTable(){var t=new uint[256];for(uint i=0;i<256;i++){uint c=i;for(int j=0;j<8;j++)c=(c>>1)^((c&1)!=0?0xEDB88320U:0);t[i]=c;}return t;}
    public static uint Crc(byte[] b){uint c=0xFFFFFFFF;foreach(byte v in b)c=(c>>8)^Table[(c^v)&255];return ~c;}
    static void Log(StreamWriter log,string s){log.WriteLine(DateTime.UtcNow.ToString("o")+" "+s);log.Flush();}
    static string Line(SerialPort port,int timeoutMs){var watch=Stopwatch.StartNew();while(watch.ElapsedMilliseconds<timeoutMs){try{return port.ReadLine().TrimEnd('\r');}catch(TimeoutException){}}throw new TimeoutException("No complete serial line");}
    static string Expect(SerialPort port,string prefix,StreamWriter log){var watch=Stopwatch.StartNew();while(watch.ElapsedMilliseconds<30000){string s=Line(port,30000);Log(log,"RX "+s);if(s.StartsWith("SDREAD ERROR"))throw new IOException(s);if(s.StartsWith(prefix))return s;}throw new IOException("Expected "+prefix);}
    static void Send(SerialPort port,string s,StreamWriter log){Log(log,"TX "+s);port.Write(s+"\r\n");}
    static byte[] Exact(SerialPort port,int length){byte[] data=new byte[length];int n=0;var watch=Stopwatch.StartNew();while(n<length){if(watch.ElapsedMilliseconds>60000)throw new TimeoutException("Incomplete binary frame");try{int got=port.Read(data,n,length-n);if(got>0)n+=got;}catch(TimeoutException){}}return data;}
    public static uint Header(string s,string name,int offset,int length){var m=Regex.Match(s,@"^SDREAD DATA name=([SP][0-9]{7}\.BIN) offset=([0-9]+) length=([0-9]+) crc32=([0-9A-F]{8})$");if(!m.Success||m.Groups[1].Value!=name||long.Parse(m.Groups[2].Value)!=offset||long.Parse(m.Groups[3].Value)!=length)throw new IOException("Frame header mismatch");return uint.Parse(m.Groups[4].Value,NumberStyles.HexNumber);}
    public static void CheckOnly(){if(Crc(Encoding.ASCII.GetBytes("123456789"))!=0xCBF43926U)throw new Exception("CRC self-test");if(Header("SDREAD DATA name=P0000000.BIN offset=0 length=3 crc32=352441C2","P0000000.BIN",0,3)!=0x352441C2U)throw new Exception("Header self-test");bool rejected=false;try{Header("SDREAD DATA name=P0000001.BIN offset=0 length=3 crc32=352441C2","P0000000.BIN",0,3);}catch(IOException){rejected=true;}if(!rejected)throw new Exception("Header guard");Console.WriteLine("PASS client CRC32 known vector and frame identity guards; no COM opened");}
    static void Help(SerialPort port,StreamWriter log){Send(port,"SDDIAG HELP",log);string s=Expect(port,"SDPERF v2 EXPORT1",log);if(!s.Contains("EXPORT INFO/READ/BAUD"))throw new IOException("Export firmware unavailable");}
    static void Baud(SerialPort port,int baud,StreamWriter log){Send(port,"SDDIAG EXPORT BAUD "+baud,log);if(Expect(port,"SDREAD BAUD ",log)!="SDREAD BAUD next="+baud)throw new IOException("Baud ACK mismatch");Thread.Sleep(50);port.BaudRate=baud;Thread.Sleep(50);Help(port,log);}
    public static void Download(string portName,int baud,string directory){
        if(baud!=115200&&baud!=460800&&baud!=921600)throw new ArgumentException("Unsupported baud");
        using(var log=new StreamWriter(new FileStream(Path.Combine(directory,"serial-export.log"),FileMode.CreateNew,FileAccess.Write),new UTF8Encoding(false)))
        using(var port=new SerialPort(portName,115200,Parity.None,8,StopBits.One)){
            port.Handshake=Handshake.None;port.DtrEnable=false;port.RtsEnable=false;port.Encoding=Encoding.ASCII;port.NewLine="\n";port.ReadTimeout=500;port.WriteTimeout=2000;
            port.Open();Log(log,"OPEN "+portName+" DTR/RTS off; no reset; no RUN commands");Thread.Sleep(500);Help(port,log);
            if(baud!=115200)Baud(port,baud,log);
            for(int i=0;i<Names.Length;i++){
                string name=Names[i];int size=Sizes[i];Send(port,"SDDIAG EXPORT INFO "+name,log);
                string info=Expect(port,"SDREAD INFO ",log);
                if(info!="SDREAD INFO name="+name+" size="+size)throw new IOException("Expected file size mismatch: "+info);
                string partial=Path.Combine(directory,name+".partial"),complete=Path.Combine(directory,name);
                var timer=Stopwatch.StartNew();
                using(var file=new FileStream(partial,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
                    for(int offset=0;offset<size;){
                        int count=Math.Min(32768,size-offset);bool valid=false;
                        for(int retry=0;retry<3;retry++){
                            Send(port,"SDDIAG EXPORT READ "+name+" "+offset+" "+count,log);
                            uint crc=Header(Expect(port,"SDREAD DATA ",log),name,offset,count);
                            byte[] data=Exact(port,count);
                            if(Line(port,30000)!=""||Line(port,30000)!="SDREAD END")throw new IOException("Binary frame trailer mismatch");
                            if(Crc(data)!=crc){Log(log,"CRC RETRY offset="+offset);continue;}
                            file.Write(data,0,data.Length);valid=true;break;
                        }
                        if(!valid)throw new IOException("Repeated transport CRC failure");
                        offset+=count;
                        if(offset%(1024*1024)==0||offset==size){string progress=name+" "+offset+"/"+size+" bytes "+timer.Elapsed.TotalSeconds.ToString("F1",CultureInfo.InvariantCulture)+"s";Console.WriteLine(progress);Log(log,progress);}
                    }
                    file.Flush(true);
                }
                File.Move(partial,complete);Log(log,"COMPLETE "+name);
            }
            if(baud!=115200)Baud(port,115200,log);
            Log(log,"ALL FILES DOWNLOADED; restoring 115200 and closing port");
        }
    }
}
