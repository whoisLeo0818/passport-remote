/* Run with Node.js. No browser or device dependency; validates the actual
 * embedded page's packing, limits and save/reconnect interactions. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('main/passport_web.html','utf8');
class Element {
 constructor(tag='div'){this.tag=tag;this.value='';this.width=1;this.height=1;this.files=[];this.children=[];this.listeners={};this.style={};this.attrs={};this.hidden=false;this.disabled=false;}
 append(e){this.children.push(e);if(this.tag==='select'&&!this.value)this.value=e.value;}
 replaceChildren(){this.children=[];}setAttribute(k,v){this.attrs[k]=v;}
 addEventListener(k,f){this.listeners[k]=f;}showModal(){this.open=true;}close(){this.open=false;}
 click(){this.onclick?.();}
 focus(){}
 getContext(){return {fillRect(){},drawImage(){},fillText(){},clearRect(){},measureText:s=>({width:[...s].length*10}),getImageData:()=>({data:new Uint8Array(this.width*this.height*4)}),createImageData:(w,h)=>({data:new Uint8Array(w*h*4)}),putImageData(){}};}
}
const ids=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(x=>[x[1],new Element(x[1]==='star'||x[1]==='zodiac'?'select':'div')]));
ids.avatar.width=118;ids.avatar.height=133;
const state={token:'local-test-token',storageReady:true,customProfile:false,photos:[],profile:{name:'测试名片',star:'天秤座',zodiac:'属兔',tag:'今天也要开心'}};
const net={enabled:false,linked:false,internet:false,connecting:false,scanBusy:false,scanValid:true,ssid:'',ip:'',networks:[{ssid:'测试网络',rssi:-45,secure:true}]};
const weather={city:0,cityName:'深圳',minutes:30,valid:false,clockSynced:false,clockCached:false,error:0};
let requests=[],sent=[],fail=false;
class XHR {
 open(method,path){this.path=path;}setRequestHeader(k,v){if(k==='X-Passport-Token')assert.equal(v,state.token);}
 upload={};send(body){sent.push({path:this.path,body});queueMicrotask(()=>{if(fail){this.onerror();return;}this.status=200;this.onload();});}
}
const c={document:{getElementById:id=>{assert(ids[id],id);return ids[id];},createElement:tag=>new Element(tag),querySelectorAll:()=>[]},window:{addEventListener(){}},setTimeout,clearTimeout,setInterval(){},AbortController,TextEncoder,DataView,Uint8Array,XMLHttpRequest:XHR,Image:class{},URL,fetch:async(path,opts)=>{requests.push({path,opts});return {ok:true,json:async()=>path.startsWith('/api/network')?net:path.startsWith('/api/weather')?weather:state};}};
vm.createContext(c);vm.runInContext(source.match(/<script>([\s\S]*?)<\/script>/)[1],c);
const run=code=>vm.runInContext(code,c);
const settle=()=>new Promise(r=>setTimeout(r,20));
(async()=>{
 await settle();assert.equal(ids.name.value,'测试名片');assert.equal(ids.status.textContent,'已连接设备');
 assert.equal(run("crc32(new TextEncoder().encode('123456789'))"),0xcbf43926);
 const b=run('packet(1,profileImages(profileData()),profileData())');const v=new DataView(b.buffer,b.byteOffset,b.byteLength);
 assert.equal(Buffer.from(b.subarray(0,4)).toString(),'PWF1');assert.equal(v.getUint32(4,true),1);assert.equal(v.getUint16(14,true),40);
 assert.equal(v.getUint32(32,true),b.length-64);assert.equal(v.getUint32(36,true),run('crc32(packet(1,profileImages(profileData()),profileData()).subarray(64))'));
 assert(b.length<65536);assert(Buffer.from(b.subarray(64,1088)).toString().includes('测试名片'));
 ids.name.value='名字超过四个汉字';assert.throws(()=>run('profileData()'),/最多/);ids.name.value='星旅同学';
 ids.tag.value='一'.repeat(17);assert.throws(()=>run('profileData()'),/最多/);ids.tag.value='每天发现一点美好';
 ids.name.listeners.input();assert.equal(ids['save-state'].textContent,'有未保存的修改');
 ids['profile-form'].onsubmit({preventDefault(){}});await settle();assert.equal(sent.length,1);assert.equal(sent[0].path,'/api/profile');assert.equal(ids['save-state'].textContent,'与设备一致');
 fail=true;ids.name.value='新的名字';ids.name.listeners.input();ids['profile-form'].onsubmit({preventDefault(){}});await settle();
 assert.equal(ids['save-state'].textContent,'有未保存的修改');assert.equal(ids.status.textContent,'未连接');
 fail=false;await run('refresh(true)');assert.equal(ids.name.value,'新的名字');assert.equal(ids.status.textContent,'已连接设备');
 await assert.rejects(run('loadImage({type:"image/gif",size:10})'),/JPG/);
 await assert.rejects(run('loadImage({type:"image/png",size:6000000})'),/5 MB/);
 run('photos=Array.from({length:6},(_,id)=>({id}));controls()');assert(ids['add-photos'].disabled);
 for(const minutes of [30,60,120,240]){ids['weather-city'].value='1';ids['weather-frequency'].value=String(minutes);await run('saveWeather()');const r=requests.filter(r=>r.path==='/api/weather'&&r.opts.method==='POST').at(-1);assert.deepEqual(JSON.parse(r.opts.body),{city:1,minutes});assert.equal(r.opts.headers['X-Passport-Token'],state.token);}
 const oldCount=requests.length;ids['weather-city'].value='2';await run('saveWeather()');assert.equal(requests.length,oldCount);
 run('chooseNetwork(networkState.networks[0])');ids['network-password'].value='short';ids['network-form'].onsubmit({preventDefault(){}});assert(ids['network-error'].textContent.includes('8–63'));
 ids['network-password'].value='test-only-password';ids['network-form'].onsubmit({preventDefault(){}});await settle();assert.equal(ids['network-password'].value,'');
 const connection=requests.filter(r=>r.path==='/api/network'&&r.opts.method==='POST').at(-1);assert.equal(JSON.parse(connection.opts.body).ssid,'测试网络');
 assert(!/<(?:script|link)[^>]+(?:src|href)="https?:/.test(source),'No external page assets');
 console.log('Passport web tests: PASS (wire format/CRC, profile limits, save failure, draft reconnect, image limits, six-photo cap)');
})().catch(e=>{console.error(e);process.exitCode=1;});
