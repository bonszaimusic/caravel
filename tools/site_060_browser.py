#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the built publication in Chromium at desktop/mobile widths."""
import argparse,json,shutil
from pathlib import Path
from playwright.sync_api import sync_playwright

def main():
    ap=argparse.ArgumentParser();ap.add_argument("site",type=Path);ap.add_argument("out",type=Path);a=ap.parse_args()
    a.out.mkdir(parents=True,exist_ok=True);results=[]
    with sync_playwright() as p:
        chrome=shutil.which("google-chrome") or shutil.which("chromium")
        browser=p.chromium.launch(headless=True,args=["--no-sandbox"],**({"executable_path":chrome} if chrome else {}))
        page=browser.new_page()
        for width in [1100,390]:
            page.set_viewport_size({"width":width,"height":900})
            for name in ["","tutorials","sounds","changelog"]:
                page.goto((a.site/name/"index.html").resolve().as_uri());page.wait_for_timeout(100)
                # A full-page review must load images that normally wait for scrolling.
                page.locator("img").evaluate_all("images=>images.forEach(i=>i.loading='eager')")
                page.wait_for_function("[...document.images].every(i=>i.complete&&i.naturalWidth>0)",timeout=10000)
                state=page.evaluate("""() => ({width:document.documentElement.scrollWidth,viewport:innerWidth,
                  images:[...document.images].every(i=>i.complete&&i.naturalWidth>0),players:document.querySelectorAll('audio').length})""")
                assert state["width"]<=width and state["images"],(name,state)
                if name=="sounds":
                    assert state["players"]==3
                    durations=page.evaluate("""async()=>Promise.all([...document.querySelectorAll('audio')].map(a=>new Promise((ok,no)=>{
                      const timer=setTimeout(()=>no(Error('Audio metadata timeout')),10000);
                      a.addEventListener('loadedmetadata',()=>{clearTimeout(timer);ok(a.duration)},{once:true});a.addEventListener('error',()=>{clearTimeout(timer);no(Error('Audio failed'))},{once:true});a.load();
                    })))""")
                    assert all(abs(x-y)<.15 for x,y in zip(durations,[20,24,24])),durations
                page.screenshot(path=str(a.out/f"{name or 'home'}-{width}.png"),full_page=True)
                results.append({"page":name or "home","width":width,**state})
        browser.close()
    (a.out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("Browser publication PASS: desktop/mobile layout, real images and three playable audio sources")

if __name__=="__main__":main()
