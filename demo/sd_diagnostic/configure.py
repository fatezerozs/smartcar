from pathlib import Path
import sys,re,xml.etree.ElementTree as E
p=Path(__file__).resolve().parent/'mdk/sd_diagnostic.uvprojx'
run=sys.argv[1]
assert re.fullmatch(r'run-[0-9T]+-[0-9]+',run)
t=E.parse(p)
c=t.find('.//TargetCommonOption')
c.find('OutputDirectory').text=('../artifacts/'+run+'/Objects/').replace('/',chr(92))
c.find('ListingPath').text=('../artifacts/'+run+'/Listings/').replace('/',chr(92))
E.indent(t,space='  ')
t.write(p,encoding='utf-8',xml_declaration=True)
