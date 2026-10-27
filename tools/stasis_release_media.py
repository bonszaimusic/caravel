#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render original STASIS demonstrations with the actual native DSP and UI."""
import hashlib,json,math,os,shlex,subprocess,wave
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/release-060-media"
AUDIO=ROOT/"docs/audio"
IMAGES=ROOT/"docs/tutorials/img"
SCENES=[
    {"id":"hold","file":"stasis-hold.mp3","title":"Hold a SEM chord","seconds":20,
     "description":"A WARM PAD chord becomes a sustained texture; MOTN animates it after the original track is muted.",
     "cues":[[0,"SEM WARM PAD chord"],[2.5,"CAPT HOLD"],[4.2,"Source track muted; captured texture remains"],[6.5,"MOTN about 79%"],[15,"STOP; natural HALL tail"]]},
    {"id":"add","file":"stasis-add.mp3","title":"Build a texture with ADD","seconds":24,
     "description":"A held SEM pad takes in DRONE SUB FOG, followed by HADES RAW LEAD. Each new source is muted after capture.",
     "cues":[[0,"SEM WARM PAD chord"],[2.5,"CAPT HOLD"],[6,"DRONE SUB FOG enters"],[7,"ADD 33%"],[9,"New source muted; overdub remains"],[12,"HADES RAW LEAD enters"],[13,"Second ADD"],[15,"Lead muted; combined texture remains"],[19,"STOP; natural HALL tail"]]},
    {"id":"blend","file":"stasis-blend.mp3","title":"Move from TIDE to OB STRING","seconds":24,
     "description":"A frozen DRONE TIDE texture transitions into a captured SEM OB STRING chord over six seconds.",
     "cues":[[0,"DRONE TIDE"],[2.5,"CAPT HOLD"],[6,"SEM OB STRING enters"],[7,"BLEND starts; TIME 6 s"],[9,"Source track muted while the captured transition continues"],[13,"New texture held"],[19,"STOP; natural HALL tail"]]},
]

def run(args,**kw):return subprocess.run([str(a) for a in args],cwd=ROOT,check=True,**kw)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    for p in [OUT,AUDIO,IMAGES]:p.mkdir(parents=True,exist_ok=True)
    flags=["-O2","-w","-ffp-contract=off","-DCARAVEL_REVERB_MINIMAL=1","-DFELUCCA_SLICE=0",
           "-Ibuild/gen","-Ifirmware/src"]
    run([*shlex.split(os.environ.get("CC","cc")),*flags,"tools/stasis_release_media.c","-lm","-o",OUT/"render"])
    results=[]
    for scene in SCENES:
        run([OUT/"render",scene["id"]])
        for name in {"hold":["stasis-live","stasis-hold"],"add":["add-ready","add-mix"],"blend":["blend-ready","blend-mix"]}[scene["id"]]:
            Image.open(OUT/(name+".ppm")).save(IMAGES/(name+".png"),optimize=True)
        wav=OUT/(scene["id"]+".wav")
        with wave.open(str(wav)) as f:
            assert f.getnchannels()==2 and f.getframerate()==44100 and f.getnframes()==scene["seconds"]*44100
            samples=np.frombuffer(f.readframes(f.getnframes()),dtype="<i2").astype(np.float64)/32768
        peak=float(np.max(np.abs(samples)));assert .001<peak<1
        gain=10**(-3/20)/peak
        path=AUDIO/scene["file"]
        run(["ffmpeg","-v","error","-y","-i",wav,"-af",f"volume={gain:.12f},afade=t=out:st={scene['seconds']-.15}:d=0.15",
             "-codec:a","libmp3lame","-q:a","3","-map_metadata","-1",path])
        decoded=run(["ffmpeg","-v","error","-i",path,"-f","f32le","-acodec","pcm_f32le","-"],capture_output=True).stdout
        signal=np.frombuffer(decoded,dtype="<f4").reshape(-1,2)
        assert np.isfinite(signal).all() and np.max(np.abs(signal))<1 and abs(len(signal)/44100-scene["seconds"])<.01
        cloud=signal[int(4.6*44100):int(5.5*44100)]
        assert float(np.sqrt(np.mean(cloud**2)))>.001,"Held texture is inaudible"
        results.append({**scene,"sha256":digest(path),"raw_wav_sha256":digest(wav),"gain_db":round(20*math.log10(gain),4),
                        "decoded_peak":round(float(np.max(np.abs(signal))),6),"held_rms":round(float(np.sqrt(np.mean(cloud**2))),6)})
    manifest={"version":"0.6.0","origin":"Native render of the actual Caravel firmware DSP; not a recording of an FM-1.",
              "processing":"Constant peak normalization to -3 dBFS and a 150-ms end fade; no external sound effects.",
              "sample_rate":44100,"channels":2,"profile":{"CARAVEL_REVERB_MINIMAL":1,"FELUCCA_SLICE":0},
              "render_source":"tools/stasis_release_media.c","clips":results}
    (AUDIO/"stasis-0.6.0.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(results,indent=2))

if __name__=="__main__":main()
