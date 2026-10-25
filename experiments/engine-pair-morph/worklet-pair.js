// SPDX-License-Identifier: GPL-3.0-only
// Included as a class method by build.py, not a separate worklet implementation.
    pairMessage(m) {
      const ex=this.ex;
      const names={prepare:"web_pair_prepare",select:"web_pair_select",morph:"web_pair_morph",off:"web_pair_off",
        panic:"web_pair_panic",control:"web_pair_control",lfo:"web_pair_lfo",lfo2_rate:"web_pair_lfo2_rate",sequence:"web_pair_sequence",motion:"web_pair_motion",enable:"web_pair_enable",page:"web_pair_page"};
      if (m.op === "save") {
        const size=ex.web_pair_snapshot_save();
        if (!size) { this.port.postMessage({type:"pair-result",message:"Prepare a pair before saving."}); return; }
        const bytes=new Uint8Array(ex.memory.buffer,ex.web_pair_snapshot_buf(),size).slice();
        this.port.postMessage({type:"pair-snapshot",bytes},[bytes.buffer]); return;
      }
      if (m.op === "restore") {
        if (!(m.bytes instanceof Uint8Array) || m.bytes.length !== ex.web_pair_snapshot_size()) return;
        new Uint8Array(ex.memory.buffer,ex.web_pair_snapshot_buf(),m.bytes.length).set(m.bytes);
        const ok=ex.web_pair_snapshot_load(m.bytes.length);
        this.port.postMessage({type:"pair-result",message:ok?"Snapshot restored; transport stopped, held notes cleared.":"Invalid or unsupported snapshot; restore refused."}); return;
      }
      const name=names[m.op]; if (!name) return;
      const result=ex[name](...(m.args||[]));
      if (m.op === "enable") this.port.postMessage({type:"pair-result",message:result?"Pair ready. Existing sounds kept; performance settings copied from A.":"Pair refused. Stop transport, release notes and check the PAIR page."});
      if (m.op === "prepare") this.port.postMessage({type:"pair-result",message:result?"Pair ready. Play either member; CC119 controls shared morph.":"Pair refused: select two distinct supported members."});
    }
