// SPDX-License-Identifier: GPL-3.0-only
    homeMessage(m){
      const ex=this.ex,args=m.args||[];
      const names={page:'web_home_open',row:'web_home_row',select:'web_pair_select',default:'web_home_default',apply:'web_home_apply',add:'web_home_add'};
      if(names[m.op]){
        ex[names[m.op]](...args);
        const template=['EMPTY','ENGINE','PERFORM'];let message='';
        if(m.op==='default')message=`Startvorlage ${template[args[0]]} gemerkt. Vorhandene HOME-Belegungen bleiben erhalten.`;
        if(m.op==='apply')message=`HOME von T${args[0]+1} durch ${template[args[1]]} ersetzt. HOME EDIT → UNDO macht es rückgängig.`;
        if(m.op==='add'){const ptr=ex.web_home_notice(),mem=new Uint8Array(ex.memory.buffer);for(let j=ptr;mem[j]&&j<ptr+128;j++)message+=String.fromCharCode(mem[j]);}
        if(message)this.port.postMessage({type:'home-result',message});return;
      }
      const slot=args[0]||0;
      if(m.op==='save'||m.op==='load'){
        const ok=ex[m.op==='save'?'web_home_save':'web_home_load'](slot);
        this.port.postMessage({type:'home-result',message:ok?`${String.fromCharCode(65+slot)}: ${m.op==='save'?(ex.web_playing()?'Im RAM gespeichert – STOP zum dauerhaften Speichern.':'Sound, HOME, Macros und Pair gespeichert.'):'Projekt geladen.'}`:'Slot leer oder Vorgang abgelehnt.'});return;
      }
      if(m.op==='export'){
        const size=ex.web_home_export(slot);if(!size){this.port.postMessage({type:'home-result',message:`Projekt ${String.fromCharCode(65+slot)} ist leer. Aktuellen Stand zuerst dort speichern.`});return;}
        const bytes=new Uint8Array(ex.memory.buffer,ex.web_home_project_buf(),size).slice();this.port.postMessage({type:'home-project',slot,bytes},[bytes.buffer]);return;
      }
      if(m.op==='import'){
        let ok=0;if(m.bytes instanceof Uint8Array&&(m.bytes.length===ex.web_home_project_size()||m.bytes.length===10000)){
          new Uint8Array(ex.memory.buffer,ex.web_home_project_buf(),m.bytes.length).set(m.bytes);ok=ex.web_home_import(m.bytes.length);
        }
        this.port.postMessage({type:'home-result',message:ok?'Projekt importiert. Mit A–D speichern dauerhaft ablegen.':'Ungültiges Projekt; Import abgelehnt.'});
      }
    }
