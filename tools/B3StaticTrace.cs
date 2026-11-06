using System;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.Threading;
using System.Collections.Generic;
public static class B3StaticTrace {
 static void Log(StreamWriter log,string s){log.WriteLine(DateTime.UtcNow.ToString("o")+" "+s);log.Flush();}
 static void Send(SerialPort p,StreamWriter log,string s){Log(log,"TX "+s);p.Write(s+"\r\n");}
 static string Line(SerialPort p,int timeout){var t=Stopwatch.StartNew();while(t.ElapsedMilliseconds<timeout){try{return p.ReadLine().TrimEnd('\r');}catch(TimeoutException){}}throw new TimeoutException("No B3 response; no reset or motion command attempted");}
 public static string Extract(string s,string prefix){int i=s.IndexOf(prefix,StringComparison.Ordinal);return i<0?null:s.Substring(i);}
 static string Expect(SerialPort p,StreamWriter log,string prefix,int timeout){var t=Stopwatch.StartNew();while(t.ElapsedMilliseconds<timeout){string s=Line(p,Math.Max(500,timeout-(int)t.ElapsedMilliseconds));Log(log,"RX "+s);string error=Extract(s,"B3TRACE ERROR");if(error!=null)throw new IOException(error);string found=Extract(s,prefix);if(found!=null)return found;}throw new TimeoutException(prefix);}
 public static Dictionary<string,string> Fields(string s){var d=new Dictionary<string,string>();foreach(Match m in Regex.Matches(s,@"(\w+)=([^\s]+)")){if(d.ContainsKey(m.Groups[1].Value))throw new IOException("Duplicate header field");d.Add(m.Groups[1].Value,m.Groups[2].Value);}return d;}
 public static bool EmptyWait(string s){var d=Fields(s);return d["count"]=="0"&&d["lost"]=="0"&&d["reason"]=="4";}
 public static void Validate(string h,int size,string source){var d=Fields(h);int n=int.Parse(d["count"]);if(!h.StartsWith("B3TRACE DATA ")||d["format"]!="B3T1"||d["static"]!="1"||d["synthetic"]!="0"||d["source"]!=source||int.Parse(d["bytes"])!=size||n<=0||n>3072||n*64!=size||size<=0||size>3072*64)throw new IOException("B3 trace identity/size mismatch");}
 public static void SelfTest(){string h="B3TRACE DATA format=B3T1 run=1 count=1 bytes=64 crc32=00000000 static=1 synthetic=0 source=test";Validate(h,64,"test");bool rejected=false;try{Validate(h,128,"test");}catch(IOException){rejected=true;}if(!rejected)throw new Exception("B3 length guard");rejected=false;try{Validate(h,64,"other");}catch(IOException){rejected=true;}if(!rejected)throw new Exception("B3 fingerprint guard");if(Extract("[ENCODER_SPEED] partial "+h,"B3TRACE DATA ")!=h||Extract("[TELEM] only","B3STATIC ")!=null)throw new Exception("Mixed diagnostic line guard");if(!EmptyWait("B3TRACE FROZEN count=0 lost=0 reason=4")||EmptyWait("B3TRACE FROZEN count=1 lost=0 reason=4")||EmptyWait("B3TRACE FROZEN count=0 lost=1 reason=4"))throw new Exception("Empty-only retry guard");Console.WriteLine("PASS B3 client header size, fingerprint, mixed-line and empty-only retry guards; no COM opened");}
 public static void Capture(string name,string output,string source,bool transition,bool downloadOnly){
  using(var log=new StreamWriter(new FileStream(Path.Combine(output,"serial.log"),FileMode.CreateNew),new UTF8Encoding(false)))
  using(var p=new SerialPort(name,115200,Parity.None,8,StopBits.One)){
   p.Handshake=Handshake.None;p.DtrEnable=false;p.RtsEnable=false;p.ReadTimeout=300;p.WriteTimeout=2000;p.NewLine="\n";p.Open();Log(log,"OPEN; static prerequisite only, no UART arm, no reset/flash");Thread.Sleep(300);
   Send(p,log,"!");Thread.Sleep(300);Send(p,log,"x");Expect(p,log,"[LINK] port=usb owner=usb",6000);
   Send(p,log,"@b3 status");string status=Expect(p,log,"B3STATIC ",5000);var id=Fields(status);
   if(id["id"]!="RECORDER_B3_STATIC_C26_1"||id["outputs"]!="forced_zero"||id["source"]!=source||id["raw_c26"]!="1"||id["mode"]!="0"||id["active"]!="0")throw new IOException("Require matching static build, C26 OFF/OBSERVE and idle; CAPTURE withheld");
   File.WriteAllText(Path.Combine(output,"identity.txt"),status,new UTF8Encoding(false));
   if(!downloadOnly){
    if(id["count"]=="0"&&id["frozen"]=="1"){Send(p,log,"@b3 clear_empty");Thread.Sleep(100);}
    for(int attempt=0;attempt<(transition?3:1);attempt++){
     if(attempt>0){Send(p,log,"@b3 clear_empty");Thread.Sleep(100);}
     Send(p,log,transition?"@b3 arm":"@b3 capture");Expect(p,log,transition?"B3TRACE ARMED ":"B3TRACE START ",5000);
     if(transition)Console.WriteLine("ARMED window "+(attempt+1)+"/3: waiting for manual C26 ON, then OFF after about one second; actuator duties remain forced zero");
     string frozen=Expect(p,log,"B3TRACE FROZEN ",transition?40000:12000);
     if(!transition||!EmptyWait(frozen))break;
     if(attempt==2)throw new TimeoutException("No C26 ON sampled in three bounded empty waiting windows; no capture accepted");
    }
   }
   Send(p,log,"@b3 status");status=Expect(p,log,"B3STATIC ",5000);id=Fields(status);
   if(id["raw_c26"]!="1"||id["mode"]!="0"||id["frozen"]!="1")throw new IOException("Capture retained in RAM; set C26 OFF before dump; no automatic reset/retry");
   Send(p,log,"@b3 dump");string header=Expect(p,log,"B3TRACE DATA ",5000);var d=Fields(header);int length=int.Parse(d["bytes"]);Validate(header,length,source);
   byte[] b=new byte[length];int at=0;var clock=Stopwatch.StartNew();
   using(var file=new FileStream(Path.Combine(output,"trace.b3t.partial"),FileMode.CreateNew,FileAccess.Write,FileShare.None)){
    while(at<length){if(clock.ElapsedMilliseconds>40000)throw new TimeoutException("Incomplete B3 binary trace");try{int n=p.Read(b,at,length-at);file.Write(b,at,n);at+=n;}catch(TimeoutException){}}file.Flush(true);
   }
   if(Line(p,5000)!=""||Line(p,5000)!="B3TRACE END")throw new IOException("B3 trailer mismatch");
   if(B2SessionExport.Crc(b)!=uint.Parse(d["crc32"],System.Globalization.NumberStyles.HexNumber))throw new IOException("B3 transport CRC mismatch; partial retained");
   File.Move(Path.Combine(output,"trace.b3t.partial"),Path.Combine(output,"trace.b3t"));File.WriteAllText(Path.Combine(output,"header.txt"),header,new UTF8Encoding(false));Log(log,"COMPLETE; CLOSE at 115200; no SD access or actuator power changes");
  }
 }
}
