#!/usr/bin/env python3
"""Read installer-exported single or bundled diagnostics; never contacts a device."""
import json,sys
root=json.load(open(sys.argv[1]))
for r in root.get('sessions',[root]):
 print('Firmware:',r.get('firmware','unknown'),'Note:',r.get('note',''))
 a=r.get('snapshots',[])
 if not a:print('No snapshots');continue
 for s in (a[0],a[-1]):
  w=s['rawWords'];v=w[119]
  print(json.dumps({'elapsed_ms':s.get('elapsedMs'),'core2_active':bool(v&1),'core2_fault':(v>>1)&3,'core2_blocks_since_boot':v>>3,'audio_wall_percent':s.get('totalPercent'),'peak_percent':s.get('peakPercent'),'late':s.get('late'),'shed':s.get('shed')},ensure_ascii=False))
print('For D1/D2: engine/track times overlap across cores. Do not add their percentages or interpret a serial restPercent as a parallel budget.')
