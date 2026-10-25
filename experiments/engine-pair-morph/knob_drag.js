// SPDX-License-Identifier: GPL-3.0-only
// Prototype-only pointer adapter. Pick one axis for the gesture; never switch it per event.
function bindKnob(k) {
  let drag = null, wheel = 0;
  k.hit.addEventListener("pointerdown", (e) => {
    if (drag || (e.button !== undefined && e.button !== 0)) return;
    e.preventDefault();
    k.hit.setPointerCapture(e.pointerId);
    drag = { id:e.pointerId, x:e.clientX, y:e.clientY, axis:null, sent:0 };
  });
  k.hit.addEventListener("pointermove", (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    e.preventDefault();
    const dx=e.clientX-drag.x, dy=drag.y-e.clientY;
    if (!drag.axis) {
      if (Math.max(Math.abs(dx),Math.abs(dy)) < 8) return;
      drag.axis = Math.abs(dx) > Math.abs(dy) ? "x" : "y";
    }
    const distance=drag.axis === "x" ? dx : dy;
    const detents=Math.trunc(distance/(k.role === "master" ? 3 : 8));
    const delta=detents-drag.sent;
    drag.sent=detents;
    if (delta) turn(k,delta);
  });
  const end=(e)=>{ if (!e || (drag && e.pointerId === drag.id)) drag=null; };
  for(const type of ["pointerup","pointercancel","lostpointercapture"]) k.hit.addEventListener(type,end);
  addEventListener("blur",()=>end());
  k.hit.addEventListener("wheel",(e)=>{
    e.preventDefault();
    const dy=e.deltaMode === 1 ? e.deltaY*33 : e.deltaY;
    if(Math.abs(dy)>=50){turn(k,dy<0?1:-1);wheel=0;return;}
    wheel-=dy;const n=Math.trunc(wheel/25);
    if(n){wheel-=n*25;turn(k,n);}
  },{passive:false});
  k.hit.addEventListener("keydown",(e)=>{
    const d={ArrowUp:1,ArrowRight:1,ArrowDown:-1,ArrowLeft:-1}[e.key];
    if(d){turn(k,d);e.preventDefault();e.stopPropagation();}
  });
}
