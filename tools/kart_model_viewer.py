#!/usr/bin/env python3
"""Local viewer for CTR racer meshes and their shared-VRAM texture palettes."""

import json
import struct
import threading
import webbrowser
import base64
from collections import defaultdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from import_custom_racer import (
    BI_RACERMODELHI,
    BI_SHAREDMPKVRM,
    CHARACTER_NAMES,
    BigFile,
    decode_template_animations,
    parse_model_file,
    parse_vram_file,
    read_u32,
)


ROOT = Path(__file__).resolve().parents[2]
BIG_PATH = ROOT / "assets" / "BIGFILE.BIG"
MODEL_COUNT = len(CHARACTER_NAMES)
VRAM_WIDTH = 1024


def bigfile_from_asset(path: Path) -> BigFile:
    """Use BigFile's normal interface with a tiny reader for standalone BIGs."""
    class AssetReader:
        def __init__(self, big_path):
            self.data = big_path.read_bytes()

        def read_range(self, _name, offset, size):
            return self.data[offset:offset + size]

    # BigFile only needs read_range for the header and entries.
    return BigFile(AssetReader(path))


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def rgb24(word):
    # CTR model colors are GPU RGB words: red is the least significant byte.
    return [word & 255, (word >> 8) & 255, (word >> 16) & 255]


def rgb15(word):
    # PS1 VRAM colors are 5:5:5 in R, G, B order. Ignore STP for display.
    return [((word >> shift) & 31) * 255 // 31 for shift in (0, 5, 10)]


def parse_vram(raw):
    pixels = {}
    for block in parse_vram_file(raw):
        for row in range(block.h):
            for col in range(block.w):
                coord = (block.y + row) * VRAM_WIDTH + block.x + col
                pixels[coord] = u16(block.pixels, (row * block.w + col) * 2)
    return pixels


def model_data(raw, name, vram_pixels):
    parsed = parse_model_file(raw)
    body = parsed["body"]
    header = parsed["model_header_offset"]
    command_offset = read_u32(body, header + 0x20)
    tex_table = read_u32(body, header + 0x28)
    color_table = read_u32(body, header + 0x2C)
    color_count = read_u32(body, command_offset)
    colors = [read_u32(body, color_table + i * 4) for i in range(color_count)]

    # The game animates these racer meshes. Use the first frame as a useful
    # preview pose; the model and palette data remain unchanged.
    animations = decode_template_animations(parsed)
    vertices = animations[0]["frames"][0]
    palette_uses = defaultdict(set)
    commands = []
    cursor = command_offset + 4
    while cursor + 4 <= len(body):
        command = read_u32(body, cursor)
        cursor += 4
        if command == 0xFFFFFFFF:
            break
        if command >> 16:
            texture_index = command & 0x1FF
            if texture_index:
                layout = read_u32(body, tex_table + (texture_index - 1) * 4)
                if layout:
                    clut = u16(body, layout + 2)
                    tpage = u16(body, layout + 6)
                    bpp = (tpage >> 7) & 3
                    if bpp < 2:
                        cx = (clut & 0x3F) * 16
                        cy = (clut >> 6) & 0x1FF
                        count = 16 if bpp == 0 else 256
                        key = (cx, cy, count)
                        palette_uses[key].add(texture_index)
            commands.append(command)

    palettes = []
    for (x, y, count), texture_ids in sorted(palette_uses.items(), key=lambda item: (item[0][1], item[0][0], item[0][2])):
        entries = []
        for slot in range(count):
            value = vram_pixels.get(y * VRAM_WIDTH + x + slot)
            if value is None:
                entries.append(None)
            else:
                entries.append({"hex": "#%02X%02X%02X" % tuple(rgb15(value)), "stp": bool(value & 0x8000)})
        palettes.append({"x": x, "y": y, "count": count, "textures": sorted(texture_ids), "entries": entries})

    # Rebuild the draw strips from the model's command stream. Cached-vertex
    # commands refer back to a prior stack slot, as they do in the game.
    mesh = []
    stack = {}
    strip = []
    vertex_index = 0
    cursor = command_offset + 4
    while cursor + 4 <= len(body):
        command = read_u32(body, cursor)
        cursor += 4
        if command == 0xFFFFFFFF:
            break
        if command >> 16 == 0:
            continue
        flags = (command >> 24) & 0xFF
        stack_index = (command >> 16) & 0xFF
        if flags & 0x80:
            strip = []
        if flags & 4:
            point = stack.get(stack_index)
        else:
            point = vertices[vertex_index] if vertex_index < len(vertices) else None
            vertex_index += 1
            if point is not None:
                stack[stack_index] = point
        if point is None:
            continue
        color_index = (command >> 9) & 0x7F
        color = rgb24(colors[color_index]) if color_index < len(colors) else [160, 160, 160]
        strip.append([point, color, command])
        if len(strip) >= 3:
            a, b, c = strip[-3:]
            if len(strip) & 1:
                a, b = b, a
            draw_command = strip[0][2] if len(strip) == 3 else command
            texture_index = draw_command & 0x1FF
            layout = read_u32(body, tex_table + (texture_index - 1) * 4) if texture_index else 0
            if not layout:
                continue
            uv = [list(struct.unpack_from("<BB", body, layout + offset)) for offset in (0, 4, 8)]
            clut = u16(body, layout + 2)
            tpage = u16(body, layout + 6)
            mode = (tpage >> 7) & 3
            if mode >= 2:
                # Racer body meshes use indexed textures; skip direct-color
                # utility triangles that do not have a palette to edit.
                continue
            page_x = (tpage & 0xF) * 64
            page_y = ((tpage >> 4) & 1) * 256
            clut_x = (clut & 0x3F) * 16
            clut_y = (clut >> 6) & 0x1FF
            mesh.append({
                "p": [a[0], b[0], c[0]], "c": [a[1], b[1], c[1]], "uv": uv,
                "page": [page_x, page_y, mode], "clut": [clut_x, clut_y],
            })

    return {
        "name": name,
        "mesh": mesh,
        "vertexCount": len(vertices),
        "colorCount": color_count,
        "modelColors": ["#%02X%02X%02X" % tuple(rgb24(value)) for value in colors],
        "palettes": palettes,
    }


def load_data():
    big = bigfile_from_asset(BIG_PATH)
    vram = parse_vram(big.read_entry(BI_SHAREDMPKVRM))
    racers = []
    for index, name in enumerate(CHARACTER_NAMES):
        racers.append(model_data(big.read_entry(BI_RACERMODELHI + index), name, vram))
    # Two bytes per VRAM word, expanded as RGBA8 (low/high byte in R/G) for
    # the WebGL shader. Holes are transparent black.
    packed_vram = bytearray(VRAM_WIDTH * 512 * 4)
    for coord, value in vram.items():
        offset = coord * 4
        packed_vram[offset] = value & 255
        packed_vram[offset + 1] = (value >> 8) & 255
        packed_vram[offset + 3] = 255
    return {"racers": racers, "vram": base64.b64encode(packed_vram).decode("ascii")}


PAGE = r'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CTR Kart Model &amp; Palette Viewer</title>
<style>:root{color-scheme:dark;--bg:#11141b;--panel:#1b202a;--line:#303846;--muted:#a8b1c2;--accent:#72d5ff}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#edf2fa;font:14px/1.45 system-ui,sans-serif}header{padding:16px 20px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:14px;flex-wrap:wrap}h1{font-size:19px;margin:0}select,input,button{background:#111722;color:#fff;border:1px solid var(--line);border-radius:7px;padding:8px 10px;font:inherit}main{display:grid;grid-template-columns:minmax(340px,1fr) minmax(420px,1.15fr);gap:14px;padding:14px;height:calc(100vh - 66px);min-height:520px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;overflow:hidden;display:flex;flex-direction:column;min-height:0}.panel h2{font-size:14px;margin:0;padding:11px 14px;border-bottom:1px solid var(--line)}.sub{color:var(--muted);font-size:12px;padding:8px 14px}.view{flex:1;min-height:200px;position:relative;background:radial-gradient(ellipse at 50% 56%,#283242,#12161d 65%)}canvas{display:block;width:100%;height:100%;touch-action:none}.hint{position:absolute;bottom:9px;left:12px;color:#b8c2d3;font-size:11px;pointer-events:none}.stats{padding:8px 14px;color:var(--muted);font-size:12px}.palette-tools{display:flex;gap:8px;padding:8px 12px;align-items:center}.palette-tools input[type=color]{padding:2px;width:42px;height:36px}.palette-tools input[type=text]{width:100px;font-family:monospace}.palettes{padding:4px 12px 14px;overflow:auto}.card{border:1px solid var(--line);border-radius:8px;margin:8px 0;overflow:hidden}.cardhead{padding:7px 9px;background:#202734;color:#dce6f4;font-size:12px;display:flex;justify-content:space-between;gap:8px}.cardhead small{color:var(--muted)}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(62px,1fr));gap:5px;padding:8px}.cell{border:1px solid #384352;border-radius:5px;overflow:hidden;cursor:pointer;background:#11151c;min-width:0}.cell:hover,.cell.selected{outline:2px solid var(--accent);outline-offset:1px}.swatch{height:26px}.label{font:10px ui-monospace,monospace;color:#c7d0df;text-align:center;padding:3px;white-space:nowrap}@media(max-width:850px){main{height:auto;min-height:0;grid-template-columns:1fr}.panel:first-child{height:55vh}.panel:last-child{height:65vh}}</style></head><body>
<header><h1>CTR Kart Model &amp; Palette Viewer</h1><select id="racer"></select><span style="color:var(--muted)">Drag to orbit · scroll to zoom</span></header><main>
<section class="panel"><h2 id="modelTitle">Textured model preview</h2><div class="view"><canvas id="model"></canvas><div class="hint">Live texture preview · click a palette slot, then edit its color</div></div><div class="stats" id="stats"></div></section>
<section class="panel"><h2>Texture palettes used by this model</h2><div class="sub">CLUT coordinates are PS1 VRAM word positions. Editing a slot updates every triangle using that palette entry.</div><div class="palette-tools"><input id="filter" placeholder="Filter color, slot, texture ID" style="flex:1"><label for="color">Color</label><input id="color" type="color" value="#ffffff"><input id="hex" type="text" value="#FFFFFF" maxlength="7"><button id="reset">Reset edits</button></div><div class="palettes" id="palettes"></div></section>
</main><script>
const DATA=__DATA__,RACERS=DATA.racers,sel=document.querySelector('#racer'),canvas=document.querySelector('#model');let current=0,rx=-.35,ry=.55,zoom=1,drag=null,chosen=null,gl,program,buf,loc={},vramBytes,originalBytes,vramTexture;
RACERS.forEach((r,i)=>{let o=document.createElement('option');o.value=i;o.textContent=r.name;sel.append(o)});
const vs=`#version 300 es\nin vec3 aPos;in vec3 aColor;in vec2 aUV;in vec3 aPage;in vec2 aClut;uniform float uRx,uRy,uZoom,uAspect;out vec3 vColor;out vec2 vUV;flat out vec3 vPage;flat out vec2 vClut;void main(){float cy=cos(uRy),sy=sin(uRy),cx=cos(uRx),sx=sin(uRx);vec3 p=vec3(aPos.x*cy-aPos.z*sy,aPos.y,aPos.x*sy+aPos.z*cy);p=vec3(p.x,p.y*cx-p.z*sx,p.y*sx+p.z*cx);gl_Position=vec4(p.x*uZoom/uAspect,p.y*uZoom,p.z*.38,1.);vColor=aColor;vUV=aUV;vPage=aPage;vClut=aClut;}`;
const fs=`#version 300 es\nprecision highp float;precision highp int;uniform sampler2D uVram;in vec3 vColor;in vec2 vUV;flat in vec3 vPage;flat in vec2 vClut;out vec4 outColor;vec2 wrapXY(int x,int y){return vec2((float((x%1024+1024)%1024)+.5)/1024.,(float((y%512+512)%512)+.5)/512.);}uint wordAt(int x,int y){vec4 p=texture(uVram,wrapXY(x,y));uint lo=uint(p.r*255.+.5),hi=uint(p.g*255.+.5);return lo|(hi<<8);}void main(){int u=int(floor(vUV.x+.5)),v=int(floor(vUV.y+.5)),px=int(vPage.x),py=int(vPage.y),mode=int(vPage.z),idx;uint raw;if(mode==0){uint w=wordAt(px+u/4,py+v);idx=int((w>>uint((u%4)*4))&15u);raw=wordAt(int(vClut.x)+idx,int(vClut.y));}else{uint w=wordAt(px+u/2,py+v);idx=int((w>>uint((u%2)*8))&255u);raw=wordAt(int(vClut.x)+idx,int(vClut.y));}vec3 texel=vec3(float(raw&31u),float((raw>>5)&31u),float((raw>>10)&31u))/31.;if(raw==0u)discard;vec3 rgb=texel*min(vColor*2.,vec3(1.));outColor=vec4(rgb,1.);}`;
function shader(type,src){let s=gl.createShader(type);gl.shaderSource(s,src);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(s));return s}
function initGL(){gl=canvas.getContext('webgl2',{antialias:true,alpha:true});if(!gl)throw Error('This browser does not support WebGL 2.');program=gl.createProgram();gl.attachShader(program,shader(gl.VERTEX_SHADER,vs));gl.attachShader(program,shader(gl.FRAGMENT_SHADER,fs));gl.linkProgram(program);if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));gl.useProgram(program);buf=gl.createBuffer();loc={pos:gl.getAttribLocation(program,'aPos'),col:gl.getAttribLocation(program,'aColor'),uv:gl.getAttribLocation(program,'aUV'),page:gl.getAttribLocation(program,'aPage'),clut:gl.getAttribLocation(program,'aClut'),rx:gl.getUniformLocation(program,'uRx'),ry:gl.getUniformLocation(program,'uRy'),zoom:gl.getUniformLocation(program,'uZoom'),aspect:gl.getUniformLocation(program,'uAspect'),vram:gl.getUniformLocation(program,'uVram')};vramTexture=gl.createTexture();gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,vramTexture);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.REPEAT);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.REPEAT);gl.uniform1i(loc.vram,0);gl.enable(gl.DEPTH_TEST);gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA)}
function buildBuffer(r){let pts=r.mesh.flatMap(t=>t.p),mn=[0,1,2].map(k=>Math.min(...pts.map(p=>p[k]))),mx=[0,1,2].map(k=>Math.max(...pts.map(p=>p[k]))),center=mn.map((x,k)=>(x+mx[k])/2),scale=1.7/Math.max(...mn.map((x,k)=>mx[k]-x),1),arr=[];for(const t of r.mesh)for(let i=0;i<3;i++){let p=t.p[i],c=t.c[i],uv=t.uv[i];arr.push((p[0]-center[0])*scale,(p[1]-center[1])*scale,(p[2]-center[2])*scale,c[0]/255,c[1]/255,c[2]/255,uv[0],uv[1],t.page[0],t.page[1],t.page[2],t.clut[0],t.clut[1])}let data=new Float32Array(arr);gl.bindBuffer(gl.ARRAY_BUFFER,buf);gl.bufferData(gl.ARRAY_BUFFER,data,gl.STATIC_DRAW);let stride=13*4;for(const [key,size,off] of [['pos',3,0],['col',3,3],['uv',2,6],['page',3,8],['clut',2,11]]){gl.enableVertexAttribArray(loc[key]);gl.vertexAttribPointer(loc[key],size,gl.FLOAT,false,stride,off*4)}gl.vertexCount=data.length/13}
function draw(){if(!gl)return;let dpr=devicePixelRatio||1,w=canvas.clientWidth,h=canvas.clientHeight;if(canvas.width!==Math.round(w*dpr)||canvas.height!==Math.round(h*dpr)){canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr)}gl.viewport(0,0,canvas.width,canvas.height);gl.clearColor(0,0,0,0);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);gl.useProgram(program);gl.uniform1f(loc.rx,rx);gl.uniform1f(loc.ry,ry);gl.uniform1f(loc.zoom,zoom);gl.uniform1f(loc.aspect,w/h);gl.bindBuffer(gl.ARRAY_BUFFER,buf);gl.drawArrays(gl.TRIANGLES,0,gl.vertexCount)}
function hexFor(rgb){return '#'+rgb.map(v=>v.toString(16).padStart(2,'0')).join('').toUpperCase()}function wordHex(w){return hexFor([Math.round((w&31)*255/31),Math.round(((w>>5)&31)*255/31),Math.round(((w>>10)&31)*255/31)])}function updateCell(x,y,slot,hex){document.querySelectorAll(`.cell[data-coord="${x},${y},${slot}"]`).forEach(c=>{c.querySelector('.swatch').style.background=hex;c.querySelector('.label').textContent=`${slot}: ${hex}`;c.dataset.search=`${hex} ${slot} ${x} ${y} ${c.dataset.tex}`})}
function setColor(hex){if(!chosen)return;hex=hex.toUpperCase();if(!/^#[0-9A-F]{6}$/.test(hex))return;let rgb=[1,3,5].map(i=>parseInt(hex.slice(i,i+2),16)),old=chosen.word,word=(old&0x8000)|((rgb[2]*31/255+.5|0)<<10)|((rgb[1]*31/255+.5|0)<<5)|(rgb[0]*31/255+.5|0),o=(chosen.y*1024+chosen.x+chosen.slot)*4;vramBytes[o]=word&255;vramBytes[o+1]=word>>8;gl.bindTexture(gl.TEXTURE_2D,vramTexture);gl.texSubImage2D(gl.TEXTURE_2D,0,chosen.x+chosen.slot,chosen.y,1,1,gl.RGBA,gl.UNSIGNED_BYTE,vramBytes.slice(o,o+4));chosen.word=word;let actual=wordHex(word);document.querySelector('#color').value=actual;document.querySelector('#hex').value=actual;updateCell(chosen.x,chosen.y,chosen.slot,actual);draw()}
function show(){let r=RACERS[current];chosen=null;document.querySelector('#modelTitle').textContent=r.name+' · textured mesh';document.querySelector('#stats').textContent=`${r.vertexCount} decoded vertices · ${r.mesh.length} textured triangles · ${r.colorCount} model vertex colors · ${r.palettes.length} texture CLUTs`;let root=document.querySelector('#palettes');root.innerHTML='';for(const p of r.palettes){let card=document.createElement('div');card.className='card';let head=document.createElement('div');head.className='cardhead';head.innerHTML=`<span>CLUT x=${p.x}, y=${p.y} · ${p.count===16?'4 bpp':'8 bpp'}</span><small>texture IDs: ${p.textures.join(', ')}</small>`;card.append(head);let grid=document.createElement('div');grid.className='grid';p.entries.forEach((e,i)=>{if(!e)return;let o=(p.y*1024+p.x+i)*4,w=vramBytes[o]|(vramBytes[o+1]<<8),color=wordHex(w),cell=document.createElement('div');cell.className='cell';cell.dataset.coord=`${p.x},${p.y},${i}`;cell.dataset.tex=p.textures.join(' ');cell.dataset.search=`${color} ${i} ${p.x} ${p.y} ${p.textures.join(' ')}`;cell.title=`${color} · slot ${i} · CLUT (${p.x}, ${p.y}) · textures ${p.textures.join(', ')}`;cell.innerHTML=`<div class="swatch" style="background:${color}"></div><div class="label">${i}: ${color}</div>`;cell.onclick=()=>{document.querySelectorAll('.cell.selected').forEach(c=>c.classList.remove('selected'));cell.classList.add('selected');chosen={x:p.x,y:p.y,slot:i,word:w};document.querySelector('#color').value=color;document.querySelector('#hex').value=color;document.querySelectorAll('.cell[data-coord="'+p.x+','+p.y+','+i+'"]').forEach(c=>c.classList.add('selected'))};grid.append(cell)});card.append(grid);root.append(card)}filter()}
function filter(){let q=document.querySelector('#filter').value.trim().toLowerCase();document.querySelectorAll('.cell').forEach(c=>c.style.display=c.dataset.search.toLowerCase().includes(q)?'':'none');document.querySelectorAll('.card').forEach(c=>c.style.display=[...c.querySelectorAll('.cell')].some(x=>x.style.display!=='none')?'':'none')}
function setup(){let raw=atob(DATA.vram);vramBytes=new Uint8Array(raw.length);for(let i=0;i<raw.length;i++)vramBytes[i]=raw.charCodeAt(i);originalBytes=vramBytes.slice();gl.bindTexture(gl.TEXTURE_2D,vramTexture);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA8,1024,512,0,gl.RGBA,gl.UNSIGNED_BYTE,vramBytes);sel.onchange=()=>{current=+sel.value;buildBuffer(RACERS[current]);show();draw()};document.querySelector('#filter').oninput=filter;document.querySelector('#color').oninput=e=>{document.querySelector('#hex').value=e.target.value.toUpperCase();setColor(e.target.value)};document.querySelector('#hex').onchange=e=>{if(/^#[0-9a-fA-F]{6}$/.test(e.target.value)){document.querySelector('#color').value=e.target.value;setColor(e.target.value)}};document.querySelector('#reset').onclick=()=>{vramBytes.set(originalBytes);gl.bindTexture(gl.TEXTURE_2D,vramTexture);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA8,1024,512,0,gl.RGBA,gl.UNSIGNED_BYTE,vramBytes);show();draw()};canvas.addEventListener('pointerdown',e=>{drag=[e.clientX,e.clientY];canvas.setPointerCapture(e.pointerId)});canvas.addEventListener('pointermove',e=>{if(!drag)return;ry+=(e.clientX-drag[0])*.009;rx+=(e.clientY-drag[1])*.009;drag=[e.clientX,e.clientY];draw()});canvas.addEventListener('pointerup',()=>drag=null);canvas.addEventListener('wheel',e=>{e.preventDefault();zoom=Math.max(.45,Math.min(2.4,zoom*(e.deltaY<0?1.08:.92)));draw()},{passive:false});window.onresize=draw;buildBuffer(RACERS[0]);show();draw()}
try{initGL();setup()}catch(e){document.querySelector('#stats').textContent='Viewer error: '+e.message;console.error(e)}
</script></body></html>'''


class Handler(BaseHTTPRequestHandler):
    data = None

    def do_GET(self):
        if self.path == "/data.json":
            payload = json.dumps(self.data, separators=(",", ":")).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        page = PAGE.replace("__DATA__", json.dumps(self.data, separators=(",", ":")))
        payload = page.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *_args):
        pass


def main():
    if not BIG_PATH.is_file():
        raise SystemExit(f"Asset not found: {BIG_PATH}")
    print("Reading racer models and shared VRAM palettes…", flush=True)
    Handler.data = load_data()
    server = ThreadingHTTPServer(("127.0.0.1", 8765), Handler)
    url = "http://127.0.0.1:8765/"
    print(f"Viewer ready at {url} (Ctrl+C to stop)", flush=True)
    threading.Timer(0.4, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nViewer stopped.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
