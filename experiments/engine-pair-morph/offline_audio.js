// SPDX-License-Identifier: GPL-3.0-only
// Local Android content:// pages cannot reliably load AudioWorklet modules.
// Reuse the exact processor and WASM through a main-thread Web Audio callback.
function createOfflineAudioNode(ac) {
  if (typeof ac.createScriptProcessor !== "function")
    throw new Error("Dieser Browser unterstützt den Offline-Audiomodus nicht. Bitte die Serverversion verwenden.");
  let Processor, currentTime = 0;
  const pagePort = {onmessage:null,postMessage(m){queueMicrotask(()=>enginePort.onmessage?.({data:m}));}};
  const enginePort = {onmessage:null,postMessage(m){queueMicrotask(()=>pagePort.onmessage?.({data:m}));}};
  class AudioWorkletProcessor { constructor(){this.port=enginePort;} }
  const registerProcessor = (_name, implementation) => { Processor=implementation; };
  /* @PROCESSOR_SOURCE@ */
  const processor = new Processor();
  const audio = ac.createScriptProcessor(1024,0,2);
  audio.onaudioprocess = (event) => {
    const left=event.outputBuffer.getChannelData(0),right=event.outputBuffer.getChannelData(1);
    for(let at=0;at<left.length;at+=128){
      const n=Math.min(128,left.length-at);
      processor.process([],[[left.subarray(at,at+n),right.subarray(at,at+n)]]);
      currentTime+=n/ac.sampleRate;
    }
  };
  return {port:pagePort,connect(destination){audio.connect(destination);},disconnect(){audio.disconnect();}};
}
