using System;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;
public static class JointStaticClient {
 static string Value(string line,string name){Match m=Regex.Match(line,@"(?:^|\s)"+Regex.Escape(name)+@"=([^\s]+)");if(!m.Success)throw new IOException("Missing "+name);return m.Groups[1].Value;}
 static void Send(SerialPort p,StreamWriter log,string s){log.WriteLine("TX "+s);log.Flush();p.Write(s+"\r\n");}
 static string Read(SerialPort p,StreamWriter log,DateTime deadline){StringBuilder b=new StringBuilder();while(DateTime.UtcNow<deadline){try{int c=p.ReadChar();if(c==10){string s=b.ToString().TrimEnd('\r');log.WriteLine("RX "+s);log.Flush();return s;}b.Append((char)c);if(b.Length>8192)throw new IOException("Oversize line");}catch(TimeoutException){}}throw new TimeoutException("Joint deadline");}
 public static void SelfTest(){if(Value("JOINT END state=4 written=100","written")!="100")throw new Exception("Parser");try{Value("JOINT END state=4","source");throw new Exception("Missing accepted");}catch(IOException){}Console.WriteLine("PASS JOINT parser; no COM opened");}
 public static void Run(string port,string output,string expected,bool c26Test){
  using(StreamWriter log=new StreamWriter(new FileStream(Path.Combine(output,"serial.log"),FileMode.CreateNew,FileAccess.Write))){
   SerialPort p=new SerialPort(port,115200,Parity.None,8,StopBits.One);p.DtrEnable=false;p.RtsEnable=false;p.Handshake=Handshake.None;p.ReadTimeout=250;p.WriteTimeout=2000;p.NewLine="\n";bool running=false;
   try{p.Open();Send(p,log,"JOINT STATUS");string id="";DateTime deadline=DateTime.UtcNow.AddSeconds(12);
    while(DateTime.UtcNow<deadline){string s=Read(p,log,deadline);if(s.Contains("JOINT id=")){id=s;break;}}
    if(Value(id,"id")!="RECORDER_JOINT_STATIC_C26_1"||Value(id,"outputs")!="forced_zero"||Value(id,"source")!=expected||Value(id,"c26")!="1")throw new IOException("Wrong firmware/source or C26 not OFF");
    if(Value(id,"camera")=="0"){
     Send(p,log,"JOINT START");System.Threading.Thread.Sleep(500);Send(p,log,"JOINT STATUS");deadline=DateTime.UtcNow.AddSeconds(15);
     do{id=Read(p,log,deadline);}while(!id.Contains("JOINT id="));
     if(Value(id,"source")!=expected||Value(id,"camera")!="1"||Value(id,"c26")!="1")throw new IOException("Camera/start readiness");
    }
    Send(p,log,c26Test?"JOINT ARM":"JOINT RUN");running=true;deadline=DateTime.UtcNow.AddSeconds(c26Test?150:35);HashSet<int> phases=new HashSet<int>();int piUpdates=0;
    for(;;){string s=Read(p,log,deadline);int at=s.IndexOf("JOINT ",StringComparison.Ordinal);if(at<0)continue;s=s.Substring(at);Console.WriteLine(s);
     if(s.StartsWith("JOINT ERROR"))throw new IOException(s);
     if(s.StartsWith("JOINT PHASE=")){int phase=int.Parse(Value(s,"PHASE"));phases.Add(phase);
      if(Value(s,"pwm_nonzero")!="0"||Value(s,"sample_not10")!="0"||Value(s,"service_budget_misses")!="0"||Value(s,"io_errors")!="0")throw new IOException("Phase timing/output/IO check failed; preserve log");
      if(phase==2)piUpdates=int.Parse(Value(s,"pi_consumed"));
     }
     if(s.StartsWith("JOINT END ")){
      int written=int.Parse(Value(s,"written"));if(Value(s,"source")!=expected||Value(s,"state")!="4"||Value(s,"error")!="0"||written<1||written>100||Value(s,"synced")!=Value(s,"written")||!phases.Contains(1)||!phases.Contains(2)||!phases.Contains(4))throw new IOException("Incomplete finite comparison");
      if(c26Test&&piUpdates<1)throw new IOException("No active PI consumption while Writer window active");
      using(StreamWriter result=new StreamWriter(new FileStream(Path.Combine(output,"end.txt"),FileMode.CreateNew,FileAccess.Write)))result.WriteLine(s);
      running=false;break;
     }
    }
   }finally{if(p.IsOpen){if(running){try{Send(p,log,"JOINT STOP");}catch(Exception){}}p.Close();}p.Dispose();}
  }
 }
}
