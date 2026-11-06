using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

public static class B2SessionExport {
 static readonly uint[] Table=BuildTable();
 static uint[] BuildTable(){var t=new uint[256];for(uint i=0;i<256;i++){uint c=i;for(int j=0;j<8;j++)c=(c>>1)^((c&1)!=0?0xEDB88320U:0);t[i]=c;}return t;}
 public static uint Crc(byte[] b){uint c=0xFFFFFFFF;foreach(byte v in b)c=(c>>8)^Table[(c^v)&255];return ~c;}
 static void Log(StreamWriter log,string s){log.WriteLine(DateTime.UtcNow.ToString("o")+" "+s);log.Flush();}
 static void Send(SerialPort p,StreamWriter log,string s){Log(log,"TX "+s);p.Write(s+"\r\n");}
 static string Line(SerialPort p){var t=Stopwatch.StartNew();while(t.ElapsedMilliseconds<15000){try{return p.ReadLine().TrimEnd('\r');}catch(TimeoutException){}}throw new TimeoutException("No complete B2 line; no reset or START attempted");}
 static string Expect(SerialPort p,StreamWriter log,string prefix){for(int i=0;i<30;i++){string s=Line(p);Log(log,"RX "+s);if(s.StartsWith("B2READ ERROR"))throw new IOException(s);if(s.StartsWith(prefix))return s;}throw new IOException("Expected "+prefix);}
 static byte[] Exact(SerialPort p,int n){var b=new byte[n];int at=0;var t=Stopwatch.StartNew();while(at<n){if(t.ElapsedMilliseconds>15000)throw new TimeoutException("Short binary payload");try{at+=p.Read(b,at,n-at);}catch(TimeoutException){}}return b;}
 public static bool Name(string s){return Regex.IsMatch(s,@"^(session\.json|[0-9]{6}\.scraw)$");}
 public static uint Header(string s,string dir,string name,int off,int n){
  var m=Regex.Match(s,@"^B2READ DATA dir=(R[0-9A-F]{7}) name=(session\.json|[0-9]{6}\.scraw) offset=([0-9]+) length=([0-9]+) crc32=([0-9A-F]{8})$");
  if(!m.Success||m.Groups[1].Value!=dir||m.Groups[2].Value!=name||long.Parse(m.Groups[3].Value)!=off||long.Parse(m.Groups[4].Value)!=n)throw new IOException("Chunk identity/offset/length mismatch");
  return uint.Parse(m.Groups[5].Value,NumberStyles.HexNumber);
 }
 public static void SelfTest(){
  if(Crc(Encoding.ASCII.GetBytes("123456789"))!=0xCBF43926U)throw new Exception("CRC vector");
  if(!Name("session.json")||!Name("000000.scraw")||Name("../session.json")||Name("P0000000.BIN"))throw new Exception("Name guard");
  string h="B2READ DATA dir=R0000001 name=000000.scraw offset=0 length=3 crc32=352441C2";
  if(Header(h,"R0000001","000000.scraw",0,3)!=0x352441C2U)throw new Exception("Header vector");
  foreach(string field in new[]{"dir=R0000001","name=000000.scraw","offset=0","length=3","crc32=352441C2"}){
   bool rejected=false;try{Header(h.Replace(field,field+"x"),"R0000001","000000.scraw",0,3);}catch(IOException){rejected=true;}if(!rejected)throw new Exception("Header mutation guard");
  }
  Console.WriteLine("PASS B2 client CRC vector, namespace and all chunk identity guards; no COM opened");
 }
 static void Baud(SerialPort p,StreamWriter log,int baud,string status){Send(p,log,"B2 EXPORT BAUD "+baud);if(Expect(p,log,"B2READ BAUD ")!="B2READ BAUD next="+baud)throw new IOException("Baud ACK");Thread.Sleep(100);p.BaudRate=baud;Thread.Sleep(100);Send(p,log,"B2 STATUS");if(Expect(p,log,"[B2] export=1 mode=")!=status)throw new IOException("Mode changed during export");}
 public static void Download(string port,string dir,string output){
  if(!Regex.IsMatch(dir,@"^R[0-9A-F]{7}$"))throw new ArgumentException("Invalid session directory");
  using(var log=new StreamWriter(new FileStream(Path.Combine(output,"serial-export.log"),FileMode.CreateNew),new UTF8Encoding(false)))
  using(var p=new SerialPort(port,115200,Parity.None,8,StopBits.One)){
   p.DtrEnable=false;p.RtsEnable=false;p.Handshake=Handshake.None;p.ReadTimeout=500;p.WriteTimeout=2000;p.NewLine="\n";
   p.Open();Log(log,"OPEN "+port+" DTR/RTS=false; readonly; never START");Thread.Sleep(500);
   Send(p,log,"B2 STATUS");string identity="";for(int i=0;i<30;i++){string s=Line(p);Log(log,"RX "+s);if(s.StartsWith("[B2] ready ")||s.StartsWith("[B2] export=1 mode=")){identity=s;break;}}
   string status;
   if(identity.Contains("id=RECORDER_CAMERA_B2_STATIC_1")&&identity.Contains("camera_started=0 session_started=0 export=1")){
    Send(p,log,"B2 EXPORT MODE");if(Expect(p,log,"B2READ MODE ")!="B2READ MODE readonly camera_started=0 session_started=0")throw new IOException("Read-only mode ACK");
    status="[B2] export=1 mode=readonly state=0 camera_started=0";
   }else if(identity=="[B2] export=1 mode=closed state=4 camera_started=1"||identity=="[B2] export=1 mode=readonly state=0 camera_started=0"){
    status=identity; // Writer is closed/unmounted, or an already-entered read-only mode.
   }else throw new IOException("Expected idle export-capable B2 or closed/unmounted Writer; nothing started");
   bool fast=false;
   try{
    Baud(p,log,921600,status);fast=true;
    Send(p,log,"B2 EXPORT LIST "+dir);if(Expect(p,log,"B2READ LIST ")!="B2READ LIST dir="+dir)throw new IOException("Directory identity mismatch");
    var names=new SortedDictionary<string,int>();
    for(int i=0;i<=257;i++){
     string s=Line(p);Log(log,"RX "+s);
     if(s.StartsWith("B2READ LISTEND ")){if(s!="B2READ LISTEND count="+names.Count)throw new IOException("Listing count");break;}
     var m=Regex.Match(s,@"^B2READ FILE name=(session\.json|[0-9]{6}\.scraw) size=([0-9]+)$");
     if(!m.Success)throw new IOException("Invalid or incomplete listing: "+s);
     string name=m.Groups[1].Value;int size=int.Parse(m.Groups[2].Value);
     if(size<=0||size>67108864||names.ContainsKey(name)||names.Count==257)throw new IOException("Invalid file size/count/duplicate");names.Add(name,size);
    }
    if(!names.ContainsKey("session.json")||!names.ContainsKey("000000.scraw"))throw new IOException("Missing complete-session manifest or first segment");
    using(var listing=new StreamWriter(new FileStream(Path.Combine(output,"listing.tsv"),FileMode.CreateNew)))foreach(var f in names)listing.WriteLine(f.Key+"\t"+f.Value);
    string source=Path.Combine(output,dir);Directory.CreateDirectory(source);
    foreach(var f in names){
     string partial=Path.Combine(source,f.Key+".partial");
     using(var file=new FileStream(partial,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
      for(int off=0;off<f.Value;){int n=Math.Min(4096,f.Value-off);bool valid=false;
       for(int retry=0;retry<3;retry++){
        Send(p,log,"B2 EXPORT READ "+dir+" "+f.Key+" "+off+" "+n);
        uint crc=Header(Expect(p,log,"B2READ DATA "),dir,f.Key,off,n);byte[] b=Exact(p,n);
        if(Line(p)!=""||Line(p)!="B2READ END")throw new IOException("Binary trailer mismatch");
        if(Crc(b)!=crc){Log(log,"CRC RETRY "+off);continue;}file.Write(b,0,b.Length);valid=true;break;
       }
       if(!valid)throw new IOException("Repeated transport CRC mismatch");off+=n;
      }
      file.Flush(true);
     }
     File.Move(partial,Path.Combine(source,f.Key));Console.WriteLine("Downloaded "+f.Key+" "+f.Value+" bytes");
    }
   }finally{if(fast){try{Baud(p,log,115200,status);Log(log,"Restored 115200");}catch(Exception e){Log(log,"Restore failed: "+e.Message);}}}
   Log(log,"CLOSE; no writes, Camera START, reset or flash");
  }
 }
}
