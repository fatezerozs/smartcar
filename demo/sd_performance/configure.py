from pathlib import Path
import sys,re,xml.etree.ElementTree as E
p=Path(__file__).resolve().parent/'mdk/sd_performance.uvprojx'
run=sys.argv[1]
assert re.fullmatch(r'run-[0-9T]+-[0-9]+',run)
t=E.parse(p)
c=t.find('.//TargetCommonOption')
c.find('OutputDirectory').text=('../artifacts/'+run+'/Objects/').replace('/',chr(92))
c.find('ListingPath').text=('../artifacts/'+run+'/Listings/').replace('/',chr(92))
E.indent(t,space='  ')
t.write(p,encoding='utf-8',xml_declaration=True)

# uVision may override linker listing location from user options.
opt=p.with_suffix('.uvoptx')
if opt.exists():
 o=E.parse(opt)
 for e in o.findall('.//ListingPath'):e.text=c.find('ListingPath').text
 for e in o.findall('.//OutputDirectory'):e.text=c.find('OutputDirectory').text
 E.indent(o,space='  ')
 o.write(opt,encoding='utf-8',xml_declaration=True)
