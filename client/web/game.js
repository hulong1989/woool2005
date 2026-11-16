const W = 1024, H = 640, TW = 64, TH = 32, CX = W / 2, CY = H / 2 + 32;
const canvas = document.getElementById("game");
const ctx = canvas.getContext("2d");
const $ = id => document.getElementById(id);
const chatEl = $("chat");
const inputEl = $("chat-input");
const charNameEl = $("char-name"), charClassEl = $("char-class");
const portraitImg = $("portrait-img"), portraitLevel = $("portrait-level");
const hpFill = $("hp-fill"), mpFill = $("mp-fill"), expFill = $("exp-fill");
const hpText = $("hp-text"), mpText = $("mp-text"), expText = $("exp-text");
const goldEl = $("char-gold"), posEl = $("char-pos"), mapTitleEl = $("map-title");
const minimap = $("minimap"), mctx = minimap.getContext("2d");
const targetFrame = $("target-frame"), targetName = $("target-name"), targetHp = $("target-hp");
const skillSlots = $("skill-slots");
const modal = $("modal"), modalTitle = $("modal-title"), modalBody = $("modal-body");
const dialogEl = $("dialog"), dialogNpc = $("dialog-npc"), dialogText = $("dialog-text");
const dialogOptions = $("dialog-options");
let dialogKey = "", dialogDismissed = "";
const questTracker = $("quest-tracker"), qtBody = $("qt-body");
const audioToggle = $("audio-toggle");
const loginScreen = $("login-screen"), loginName = $("login-name");
const loginPass = $("login-pass"), loginClass = $("login-class"), loginMsg = $("login-msg");
const logoutBtn = $("logout-btn");
const charScreen = $("char-screen"), charList = $("char-list"), charAccount = $("char-account");
const charName = $("char-name"), charClass = $("char-class"), charMsg = $("char-msg");
let authToken = "";
const deathEl = $("death"), deathSub = $("death-sub");
const mapBanner = $("map-banner");
let lastMapLabel = "";

let S = null;
let monMap = {}, playerMap = {}, npcFiles = [], iconMap = {};
let floorImgs = {}, sprImgs = {}, iconImgs = {}, sprFrames = {};
let chatCount = 0;
let cooldowns = {};
let skillButtons = [];
let lastState = 0;
let moveGoal = null, holdGoal = null, holding = false;
let targetId = 0, hoverTile = null;
let lastStep = 0, lastAttack = 0, stuck = 0, lastPos = { x: 0, y: 0 };
let potionButtons = [];
let currentPanel = null;
let chatTab = "all";
let lastLevel = 0;
let localFx = [];

const DIRS = { ArrowUp: 0, ArrowRight: 2, ArrowDown: 4, ArrowLeft: 6,
  w: 0, d: 2, s: 4, a: 6 };

// ---------------------------------------------------------------- 音频
let AC = null, audioOn = false, bgmTimer = null, lastHitSfx = 0;
const NOTES = [220, 261.63, 293.66, 329.63, 392, 440, 392, 329.63];

function ensureAudio() {
  if (!AC) {
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (Ctx) AC = new Ctx();
  }
  if (AC && AC.state === "suspended") AC.resume();
}

function tone(freq, dur, type, vol, delay) {
  if (!audioOn || !AC) return;
  const t0 = AC.currentTime + (delay || 0);
  const osc = AC.createOscillator(), gain = AC.createGain();
  osc.type = type || "sine";
  osc.frequency.setValueAtTime(freq, t0);
  gain.gain.setValueAtTime(0.0001, t0);
  gain.gain.exponentialRampToValueAtTime(vol || 0.04, t0 + 0.02);
  gain.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
  osc.connect(gain); gain.connect(AC.destination);
  osc.start(t0); osc.stop(t0 + dur + 0.05);
}

function sfx(kind) {
  if (!audioOn) return;
  ensureAudio();
  if (kind === "ui") tone(520, 0.07, "square", 0.02);
  else if (kind === "step") tone(150, 0.06, "triangle", 0.02);
  else if (kind === "attack") { tone(180, 0.12, "sawtooth", 0.035); tone(90, 0.18, "triangle", 0.03, 0.04); }
  else if (kind === "hit") {
    const now = Date.now();
    if (now - lastHitSfx < 180) return;
    lastHitSfx = now;
    tone(110, 0.12, "square", 0.03); tone(70, 0.2, "sawtooth", 0.02, 0.03);
  }
  else if (kind === "spell") { tone(660, 0.2, "sine", 0.03); tone(990, 0.3, "sine", 0.02, 0.05); }
  else if (kind === "level") { [523, 659, 784, 1046].forEach((f, i) => tone(f, 0.25, "sine", 0.04, i * 0.12)); }
}

function startBgm() {
  if (bgmTimer) return;
  let i = 0;
  bgmTimer = setInterval(() => {
    if (!audioOn) return;
    ensureAudio();
    tone(NOTES[i++ % NOTES.length] / 2, 1.9, "triangle", 0.012);
    if (i % 8 === 0) tone(NOTES[0] / 2, 2.4, "sine", 0.01);
  }, 1700);
}
function stopBgm() { if (bgmTimer) { clearInterval(bgmTimer); bgmTimer = null; } }
function toggleAudio() {
  audioOn = !audioOn;
  if (audioOn) { ensureAudio(); startBgm(); audioToggle.textContent = "音效：开"; }
  else { stopBgm(); audioToggle.textContent = "音效：关"; }
  try { localStorage.setItem("woool_audio", audioOn ? "1" : "0"); } catch (e) {}
}

// ---------------------------------------------------------------- 动画
const animMap = new Map();
let selfAttackUntil = 0;
function animFor(id, x, y) {
  let a = animMap.get(id);
  if (!a) { a = { x, y, moveUntil: 0 }; animMap.set(id, a); }
  if (a.x !== x || a.y !== y) { a.moveUntil = Date.now() + 240; a.x = x; a.y = y; }
  return a;
}
function dirOffset(dir, dist) {
  const d = { 0: [0, -1], 1: [0.7, -0.7], 2: [1, 0], 3: [0.7, 0.7],
              4: [0, 1], 5: [-0.7, 0.7], 6: [-1, 0], 7: [-0.7, -0.7] }[dir] || [0, 0];
  return [d[0] * dist, d[1] * dist];
}

function loadImg(src) {
  const im = new Image();
  im.src = src;
  return im;
}

async function loadAssets() {
  floorImgs = {
    grass: loadImg("/assets/thirdparty/stone_soup/grass.gif"),
    grass2: loadImg("/assets/thirdparty/stone_soup/grass2.gif"),
    grass3: loadImg("/assets/thirdparty/stone_soup/grass3.gif"),
    crypt: loadImg("/assets/thirdparty/stone_soup/crypt.gif"),
    crypt2: loadImg("/assets/thirdparty/stone_soup/crypt2.gif"),
    crypt3: loadImg("/assets/thirdparty/stone_soup/crypt3.gif"),
    wall: loadImg("/assets/thirdparty/stone_soup/wall.gif"),
    wall2: loadImg("/assets/thirdparty/stone_soup/wall2.gif"),
  };
  try {
    const man = await (await fetch("/assets/thirdparty/dcss/sprites.json")).json();
    monMap = man.monsters || {};
    playerMap = man.players || {};
    npcFiles = man.npc || [];
    iconMap = man.icons || {};
    for (const fn of new Set([...Object.values(monMap), ...Object.values(playerMap),
                              ...npcFiles, ...Object.values(iconMap)])) {
      sprImgs[fn] = loadImg("/assets/thirdparty/dcss/" + fn);
    }
    try {
      const fm = await (await fetch("/assets/thirdparty/dcss_frames/frames.json")).json();
      for (const [gif, files] of Object.entries(fm)) {
        sprFrames[gif] = files.map(f => loadImg("/assets/thirdparty/dcss_frames/" + f));
      }
    } catch (e) { console.log(e); }
  } catch (e) { console.log(e); }
}

async function cmd(text) {
  try {
    if (!authToken) return;
    await fetch("/cmd", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Token": authToken },
      body: JSON.stringify({ text }),
    });
  } catch (e) {}
}

function iso(wx, wy) {
  const dx = wx - S.x, dy = wy - S.y;
  return [CX + (dx - dy) * TW / 2, CY + (dx + dy) * TH / 2];
}

const smoothPos = new Map();
const cam = { x: 0, y: 0, ready: false };

function smoothXY(id, tx, ty) {
  let p = smoothPos.get(id);
  if (!p) { p = { x: tx, y: ty }; smoothPos.set(id, p); }
  if (Math.hypot(tx - p.x, ty - p.y) > 10) { p.x = tx; p.y = ty; return p; }
  p.x += (tx - p.x) * 0.30;
  p.y += (ty - p.y) * 0.30;
  if (Math.abs(tx - p.x) < 0.012) p.x = tx;
  if (Math.abs(ty - p.y) < 0.012) p.y = ty;
  return p;
}

function isoAt(wx, wy) {
  const dx = wx - cam.x, dy = wy - cam.y;
  return [CX + (dx - dy) * TW / 2, CY + (dx + dy) * TH / 2];
}

function terrainAt(wx, wy) {
  const t = S.terrain;
  if (!t) return 0;
  const x = wx - t.ox, y = wy - t.oy;
  if (x < 0 || y < 0 || x >= t.w || y >= t.h) return 2;
  const i = y * t.w + x;
  return i < t.blocks.length ? t.blocks[i] : 2;
}

function spriteFileFor(ent) {
  if (ent.type === 3) {
    let name = ent.name;
    return monMap[name] || monMap[name.split("(")[0]] || null;
  }
  if (ent.type === 4) {
    if (!npcFiles.length) return null;
    let h = 0;
    for (const c of ent.name) h += c.charCodeAt(0);
    return npcFiles[h % npcFiles.length];
  }
  if (ent.type === 5) {
    return playerMap[String(S.player_class)] || playerMap["0"] || null;
  }
  return null;
}
function spriteFor(ent) {
  const fn = spriteFileFor(ent);
  return fn ? sprImgs[fn] : null;
}
function frameImage(fn, frame) {
  const arr = sprFrames[fn];
  if (arr && arr[frame] && arr[frame].complete) return arr[frame];
  return null;
}

function bar(x, y, w, h, ratio, color) {
  ratio = Math.max(0, Math.min(1, ratio));
  ctx.fillStyle = "#241a10"; ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = "#5d4a2a"; ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  ctx.fillStyle = color; ctx.fillRect(x + 1, y + 1, (w - 2) * ratio, h - 2);
}

function drawMinimap() {
  const mx = W - 196, my = 10, mw = 184, mh = 128;
  ctx.fillStyle = "rgba(18,15,10,.92)";
  ctx.fillRect(mx, my, mw, mh);
  ctx.strokeStyle = "#a8843f"; ctx.lineWidth = 2;
  ctx.strokeRect(mx + 0.5, my + 0.5, mw - 1, mh - 1);
  ctx.lineWidth = 1;
  const t = S.terrain;
  if (t) {
    const sc = Math.min((mw - 12) / t.w, (mh - 12) / t.h);
    const ox = mx + (mw - t.w * sc) / 2, oy = my + (mh - t.h * sc) / 2;
    for (let y = 0; y < t.h; y += 2) {
      for (let x = 0; x < t.w; x += 2) {
        const v = t.blocks[y * t.w + x];
        if (v === 2) continue;
        ctx.fillStyle = v === 1 ? "#4a4038" : "#33513a";
        ctx.fillRect(ox + x * sc, oy + y * sc, 2 * sc, 2 * sc);
      }
    }
    for (const e of S.entities) {
      ctx.fillStyle = e.type === 3 ? "#d65a3f" : (e.type === 4 ? "#e8c37a" : "#5aa9e6");
      ctx.beginPath();
      ctx.arc(ox + (e.x - t.ox) * sc, oy + (e.y - t.oy) * sc, 1.5, 0, 6.283);
      ctx.fill();
    }
    ctx.fillStyle = "#fff";
    ctx.beginPath();
    ctx.arc(ox + (S.x - t.ox) * sc, oy + (S.y - t.oy) * sc, 3, 0, 6.283);
    ctx.fill();
  }
  ctx.fillStyle = "#a8843f";
  ctx.font = "11px 'Songti SC', serif";
  ctx.fillText(S.title || S.map || "", mx + 8, my + 16);
}

function drawHud() {
  ctx.fillStyle = "rgba(18,15,10,.92)";
  ctx.fillRect(10, 10, 310, 128);
  ctx.strokeStyle = "#a8843f"; ctx.lineWidth = 2;
  ctx.strokeRect(10, 10, 310, 128);
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(58, 58, 28, 0, 6.283);
  ctx.fillStyle = "#2a2014"; ctx.fill();
  ctx.strokeStyle = "#c9a24a"; ctx.stroke();
  ctx.fillStyle = "#f0d9a0";
  ctx.font = "bold 14px 'Songti SC', serif";
  ctx.fillText(S.name || "无名", 96, 30);
  ctx.font = "11px 'Songti SC', serif";
  ctx.fillStyle = "#c9a24a";
  ctx.fillText("Lv." + S.level + "  " + (S.title || ""), 96, 48);
  bar(96, 60, 200, 10, S.hp / Math.max(1, S.maxhp), "#a33131");
  bar(96, 78, 200, 10, S.mp / Math.max(1, S.maxmp), "#2f4f9e");
  bar(96, 96, 200, 8, S.exp / Math.max(1, S.maxexp), "#c9a24a");
  ctx.fillStyle = "#fff";
  ctx.font = "9px Menlo, monospace";
  ctx.fillText(S.hp + "/" + S.maxhp, 200, 69);
  ctx.fillText(S.mp + "/" + S.maxmp, 200, 87);
  ctx.fillStyle = "#d9b36c";
  ctx.font = "11px 'Songti SC', serif";
  ctx.fillText("金币 " + S.gold + "    " + S.x + "," + S.y, 96, 122);
  drawMinimap();
}

function draw() {
  ctx.clearRect(0, 0, W, H);
  if (!S) { requestAnimationFrame(draw); return; }

  const title = S.title || S.map || "";
  const dungeon = /[殿谷寺坟洞地刹矿阁]|魔龙|通天|炼狱|魔域|神舰|海底|暗殿|幻境|屠龙|巢穴|祭坛|皇宫/.test(title);

  const sp = smoothXY("self", S.x, S.y);
  if (!cam.ready) { cam.x = sp.x; cam.y = sp.y; cam.ready = true; }
  cam.x = sp.x; cam.y = sp.y;
  const baseX = Math.floor(cam.x), baseY = Math.floor(cam.y);

  for (let sy = -21; sy <= 21; sy++) {
    for (let sx = -21; sx <= 21; sx++) {
      const wx = baseX + sx, wy = baseY + sy;
      const px = CX + ((wx - cam.x) - (wy - cam.y)) * TW / 2;
      const py = CY + ((wx - cam.x) + (wy - cam.y)) * TH / 2;
      if (px < -TW || px > W + TW || py < -TH || py > H + TH) continue;
      const block = terrainAt(wx, wy);
      if (block !== 2) {
        const keys = dungeon ? ["crypt_lit", "crypt_lit", "crypt_lit"] : ["grass", "grass2", "grass3"];
        const key = keys[(wx * 7 + wy * 13) % 3];
        const img = floorImgs[key];
        if (img && img.complete) ctx.drawImage(img, px - TW / 2, py - TH / 2, TW, TH);
        else {
          ctx.fillStyle = block === 1 ? "#4a4038" : "#2f4a33";
          ctx.beginPath();
          ctx.moveTo(px, py - TH / 2); ctx.lineTo(px + TW / 2, py);
          ctx.lineTo(px, py + TH / 2); ctx.lineTo(px - TW / 2, py);
          ctx.closePath(); ctx.fill();
        }
      }
      if (block === 1) {
        const wk = (wx * 3 + wy * 5) % 2 ? "wall2" : "wall";
        const img = floorImgs[wk];
        if (img && img.complete) ctx.drawImage(img, px - TW / 2, py + TH / 2 - 64, TW, 64);
      }
    }
  }

  const ents = [...S.entities].sort((a, b) => (a.x + a.y) - (b.x + b.y));
  for (const e of ents) {
    const ep = smoothXY("e" + e.id, e.x, e.y);
    const ea = animFor("e" + e.id, e.x, e.y);
    const moveBob = ea.moveUntil > Date.now() ? Math.abs(Math.sin(performance.now() / 90)) * 3 : 0;
    const [sx, sy] = isoAt(ep.x, ep.y);
    if (sx < -60 || sx > W + 60 || sy < -60 || sy > H + 60) continue;
    ctx.fillStyle = "rgba(20,14,8,.6)";
    ctx.beginPath(); ctx.ellipse(sx, sy + 10, 15, 5, 0, 0, 6.283); ctx.fill();
    const sfn = spriteFileFor(e);
    const moving = ea.moveUntil > Date.now();
    const fidx = moving ? 1 + (Math.floor(performance.now() / 150) % 2) : 0;
    const img = frameImage(sfn, fidx) || spriteFor(e);
    if (img && img.complete) {
      ctx.drawImage(img, sx - 32, sy + 12 - 64 - moveBob, 64, 64);
    } else {
      ctx.fillStyle = e.type === 3 ? "#a8432f" : (e.type === 4 ? "#c9a24a" : "#3f6f9a");
      ctx.fillRect(sx - 10, sy - 24, 20, 30);
    }
    if (e.type === 3 && e.maxhp > 0) {
      const r = Math.max(0, Math.min(1, e.hp / e.maxhp));
      ctx.fillStyle = "#2a0c08"; ctx.fillRect(sx - 14, sy - 40, 28, 4);
      ctx.fillStyle = "#d65a3f"; ctx.fillRect(sx - 14, sy - 40, 28 * r, 4);
    }
    ctx.fillStyle = "#e8dcc0";
    ctx.font = "10px 'Songti SC', serif";
    ctx.textAlign = "center";
    ctx.fillText(e.name.slice(0, 6), sx, sy + 22);
    ctx.textAlign = "left";
  }

  const [px, py] = [CX, CY];
  const sa = animFor("self", S.x, S.y);
  const selfBob = sa.moveUntil > Date.now() ? Math.abs(Math.sin(performance.now() / 90)) * 3 : 0;
  let lungeX = 0, lungeY = 0;
  if (Date.now() < selfAttackUntil) {
    const off = dirOffset(S.dir, 6);
    lungeX = off[0]; lungeY = off[1];
  }
  ctx.fillStyle = "rgba(20,14,8,.6)";
  ctx.beginPath(); ctx.ellipse(px, py + 10, 16, 5, 0, 0, 6.283); ctx.fill();
  const ownFn = playerMap[String(S.player_class)] || playerMap["0"];
  let ownIdx = 0;
  if (Date.now() < selfAttackUntil) ownIdx = 2;
  else if (sa.moveUntil > Date.now()) ownIdx = 1 + (Math.floor(performance.now() / 150) % 2);
  const own = frameImage(ownFn, ownIdx) || sprImgs[ownFn];
  if (own && own.complete) ctx.drawImage(own, px + lungeX - 32, py + lungeY + 12 - 64 - selfBob, 64, 64);
  else { ctx.fillStyle = "#3f6f9a"; ctx.fillRect(px - 10, py - 24, 20, 30); }
  ctx.fillStyle = "#f0d9a0"; ctx.font = "bold 11px 'Songti SC', serif";
  ctx.textAlign = "center"; ctx.fillText(S.name, px, py + 26); ctx.textAlign = "left";

  //	伤害/法术飘字
  for (const ef of (S.effects || [])) {
    const [ex, ey] = isoAt(ef.x, ef.y);
    const life = Math.max(0, Math.min(1, ef.left / 0.8));
    if (ef.kind === "damage") {
      ctx.globalAlpha = life;
      ctx.fillStyle = ef.color || "#ff6666";
      ctx.font = "bold 14px 'Songti SC', serif";
      ctx.textAlign = "center";
      ctx.fillText(ef.text, ex, ey - 34 - (1 - life) * 32);
      ctx.textAlign = "left";
      ctx.globalAlpha = 1;
    } else {
      const r = 14 + (1 - life) * 34;
      ctx.globalAlpha = life * 0.8;
      ctx.strokeStyle = ef.color || "#7fd0ff";
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(ex, ey - 16, r, 0, 6.283);
      ctx.stroke();
      ctx.lineWidth = 1;
      ctx.globalAlpha = 1;
    }
  }
  const nowFx = Date.now();
  localFx = localFx.filter(f => f.until > nowFx);
  for (const f of localFx) {
    const life = Math.max(0, Math.min(1, (f.until - nowFx) / 1200));
    const [fx, fy] = isoAt(f.x, f.y);
    ctx.globalAlpha = life;
    ctx.fillStyle = f.color;
    ctx.font = "bold 16px 'Songti SC', serif";
    ctx.textAlign = "center";
    ctx.fillText(f.text, fx, fy - 50 - (1 - life) * 30);
    ctx.textAlign = "left";
    ctx.globalAlpha = 1;
  }

  //	目标/目的地/悬停
  if (hoverTile) {
    const [hx, hy] = isoAt(hoverTile.x, hoverTile.y);
    ctx.strokeStyle = entityAt(hoverTile.x, hoverTile.y) ? "#d65a3f" : "#c9a24a";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(hx, hy - TH / 2); ctx.lineTo(hx + TW / 2, hy);
    ctx.lineTo(hx, hy + TH / 2); ctx.lineTo(hx - TW / 2, hy);
    ctx.closePath(); ctx.stroke();
    ctx.lineWidth = 1;
  }
  if (targetId) {
    const t = S.entities.find(e => e.id === targetId);
    if (t) {
      const tp = smoothXY("e" + t.id, t.x, t.y);
      const [tx2, ty2] = isoAt(tp.x, tp.y);
      ctx.strokeStyle = "#ff5555"; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.ellipse(tx2, ty2 + 8, 18, 8, 0, 0, 6.283); ctx.stroke();
      ctx.lineWidth = 1;
    }
  }
  if (moveGoal && moveGoal.type === "move") {
    const [gx, gy] = isoAt(moveGoal.x, moveGoal.y);
    ctx.strokeStyle = "#7fd6a8"; ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(gx - 8, gy - 8); ctx.lineTo(gx + 8, gy + 8);
    ctx.moveTo(gx + 8, gy - 8); ctx.lineTo(gx - 8, gy + 8);
    ctx.stroke(); ctx.lineWidth = 1;
  }

  requestAnimationFrame(draw);
}

function esc(v) {
  return String(v == null ? "" : v).replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  }[c]));
}

function renderChat() {
  if (!S) return;
  const lines = S.chat || [];
  const html = [];
  for (const line of lines) {
    let show = true;
    if (chatTab === "sys") show = /系统|公告|GM|提示/.test(line);
    else if (chatTab === "fight") show = /伤害|攻击|经验|升级|死亡|击杀|命中/.test(line);
    if (!show) continue;
    let cls = "";
    if (line.includes("【公告】")) cls = "warn";
    else if (line.includes("系统") || line.includes("GM")) cls = "sys";
    else if (/伤害|攻击|命中/.test(line)) cls = "fight";
    html.push(`<div class="${cls}">${esc(line)}</div>`);
  }
  chatEl.innerHTML = html.join("");
  chatEl.scrollTop = chatEl.scrollHeight;
}

document.querySelectorAll(".chat-tabs button").forEach(b => {
  b.onclick = () => {
    chatTab = b.dataset.tab;
    document.querySelectorAll(".chat-tabs button").forEach(x =>
      x.classList.toggle("active", x === b));
    renderChat();
  };
});

function renderSkills() {
  if (!S) return;
  const skills = (S.skills || []).slice(0, 8);
  skillSlots.innerHTML = "";
  skills.forEach((sk, i) => {
    const div = document.createElement("div");
    const left = (cooldowns[sk.id] || 0) - Date.now();
    const usable = sk.learned && S.level >= sk.need && S.mp >= sk.mp && left <= 0;
    div.className = "slot" + (usable ? "" : " cool");
    div.innerHTML = `<span>F${i + 1}</span><small>${esc(sk.name)}</small>` +
      (left > 0 ? `<i class="cd">${(left / 1000).toFixed(1)}</i>` : "");
    div.onclick = () => cast(sk);
    skillSlots.appendChild(div);
  });
  potionList().slice(0, 2).forEach((p, i) => {
    const div = document.createElement("div");
    div.className = "slot potion";
    div.innerHTML = `<span>${i + 1}</span><small>${esc(p.name.slice(0, 2))}</small><b>${p.count}</b>`;
    div.title = p.name + " x" + p.count;
    div.onclick = () => usePotion(i);
    skillSlots.appendChild(div);
  });
}

function potionList() {
  return (S && S.bag ? S.bag : []).filter(i => i.name.indexOf("药") >= 0);
}

function usePotion(n) {
  const list = potionList();
  if (list[n]) cmd("use " + list[n].id);
}

function talkTo(npc) {
  dialogDismissed = "";
  cmd("talk " + npc);
}

function closeDialog() {
  dialogDismissed = dialogKey;
  dialogEl.classList.add("hidden");
}

function renderDialog() {
  if (!S) return;
  const d = S.dialog;
  if (!d) { dialogEl.classList.add("hidden"); return; }
  const key = d.npc + "|" + d.text + "|" + JSON.stringify(d.options || []);
  if (key === dialogDismissed) { dialogEl.classList.add("hidden"); return; }
  if (key !== dialogKey) {
    dialogKey = key;
    dialogNpc.textContent = d.npc || "NPC";
    dialogText.textContent = d.text || "";
    dialogOptions.innerHTML = "";
    (d.options || []).forEach(opt => {
      const b = document.createElement("button");
      b.textContent = opt.index + ". " + opt.text;
      b.onclick = () => {
        closeDialog();
        if (opt.action === "shop") { openPanel("shop"); cmd("shop"); }
        else if (opt.action === "quests") { openPanel("quest"); cmd("questlog"); }
        else if (opt.action === "close") { /* 关闭 */ }
        else if (opt.action === "teleport") { cmd(`choose ${d.npc} ${opt.index}`); }
        else { cmd(`choose ${d.npc} ${opt.index}`); }
      };
      dialogOptions.appendChild(b);
    });
  }
  dialogEl.classList.remove("hidden");
}

$("dialog-close").onclick = closeDialog;

function cast(sk) {
  if (!sk.learned || S.level < sk.need || (cooldowns[sk.id] || 0) > Date.now()) return;
  cmd("cast " + sk.name);
  selfAttackUntil = Date.now() + 300;
  sfx("spell");
  cooldowns[sk.id] = Date.now() + (sk.cd || 0);
}

const CLASS_NAME = { 0: "战士", 1: "法师", 2: "道士" };

function updateHud() {
  if (!S) return;
  charNameEl.textContent = S.name || "无名";
  charClassEl.textContent = CLASS_NAME[S.player_class] || "";
  portraitLevel.textContent = "Lv." + S.level;
  const pfn = playerMap[String(S.player_class)] || playerMap["0"];
  if (pfn) {
    const src = "/assets/thirdparty/dcss/" + pfn;
    if (portraitImg.getAttribute("src") !== src) portraitImg.src = src;
  }
  hpFill.style.width = Math.max(0, Math.min(100, S.hp / Math.max(1, S.maxhp) * 100)) + "%";
  mpFill.style.width = Math.max(0, Math.min(100, S.mp / Math.max(1, S.maxmp) * 100)) + "%";
  expFill.style.width = Math.max(0, Math.min(100, S.exp / Math.max(1, S.maxexp) * 100)) + "%";
  hpText.textContent = S.hp + "/" + S.maxhp;
  mpText.textContent = S.mp + "/" + S.maxmp;
  expText.textContent = S.exp + "/" + S.maxexp;
  goldEl.textContent = "金币 " + S.gold;
  posEl.textContent = S.x + "," + S.y;
  mapTitleEl.textContent = S.title || S.map || "";

  const t = targetId ? S.entities.find(e => e.id === targetId) : null;
  if (t) {
    targetFrame.classList.remove("hidden");
    targetName.textContent = t.name;
    targetHp.style.width = (t.maxhp > 0 ? t.hp / t.maxhp * 100 : 0) + "%";
  } else {
    targetFrame.classList.add("hidden");
  }
}

function renderQuestTracker() {
  if (!S) return;
  const active = (S.quests || []).filter(q => q.state === 1);
  if (!active.length) { questTracker.classList.add("hidden"); return; }
  questTracker.classList.remove("hidden");
  qtBody.innerHTML = active.slice(0, 4).map(q =>
    `<div class="qt-quest"><b>${esc(q.name)}</b><br><em>${esc(q.target)} ${q.progress}/${q.need}</em></div>`
  ).join("");
}

function renderDeath() {
  if (!S) return;
  const left = S.death_left || 0;
  if (left > 0) {
    deathEl.classList.remove("hidden");
    deathSub.textContent = "回城复活 " + left.toFixed(1) + "s";
  } else {
    deathEl.classList.add("hidden");
  }
}

function checkMapChange() {
  if (!S) return;
  const label = S.title || S.map || "";
  if (lastMapLabel && label && label !== lastMapLabel) {
    mapBanner.textContent = "进入 · " + label;
    mapBanner.classList.remove("hidden");
    mapBanner.classList.add("show");
    setTimeout(() => mapBanner.classList.remove("show"), 1500);
    setTimeout(() => mapBanner.classList.add("hidden"), 2000);
  }
  lastMapLabel = label;
}

function drawMinimapDOM() {
  if (!S) return;
  const w = minimap.width, h = minimap.height;
  mctx.clearRect(0, 0, w, h);
  mctx.fillStyle = "#0a0906"; mctx.fillRect(0, 0, w, h);
  const t = S.terrain;
  if (t) {
    const sc = Math.min((w - 8) / t.w, (h - 8) / t.h);
    const ox = (w - t.w * sc) / 2, oy = (h - t.h * sc) / 2;
    for (let y = 0; y < t.h; y += 2) {
      for (let x = 0; x < t.w; x += 2) {
        const v = t.blocks[y * t.w + x];
        if (v === 2) continue;
        mctx.fillStyle = v === 1 ? "#4a4038" : "#33513a";
        mctx.fillRect(ox + x * sc, oy + y * sc, 2 * sc, 2 * sc);
      }
    }
    for (const e of S.entities) {
      mctx.fillStyle = e.type === 3 ? "#d65a3f" : (e.type === 4 ? "#e8c37a" : "#5aa9e6");
      mctx.beginPath();
      mctx.arc(ox + (e.x - t.ox) * sc, oy + (e.y - t.oy) * sc, 1.5, 0, 6.283);
      mctx.fill();
    }
    mctx.fillStyle = "#fff";
    mctx.beginPath();
    mctx.arc(ox + (S.x - t.ox) * sc, oy + (S.y - t.oy) * sc, 2.5, 0, 6.283);
    mctx.fill();
  }
}

function iconFor(name) {
  let key = "misc";
  if (/剑|刀/.test(name)) key = "sword";
  else if (/斧/.test(name)) key = "axe";
  else if (/杖|扇/.test(name)) key = "staff";
  else if (/盔/.test(name)) key = "helmet";
  else if (/袍|道士服/.test(name)) key = "robe";
  else if (/甲|衣|服/.test(name)) key = "armor";
  else if (/项链|铃/.test(name)) key = "amulet";
  else if (/戒指|手镯/.test(name)) key = "ring";
  else if (/魔法/.test(name)) key = "potion_mp";
  else if (/药/.test(name)) key = "potion_hp";
  else if (/书/.test(name)) key = "book";
  else if (/金币/.test(name)) key = "gold";
  const fn = iconMap[key] || iconMap.misc;
  return fn ? "/assets/thirdparty/dcss/" + fn : "";
}

function openPanel(kind) {
  sfx("ui");
  closeDialog();
  currentPanel = kind;
  modal.classList.remove("hidden");
  const titles = { char: "角色", bag: "背包", skill: "技能", quest: "任务", party: "队伍", dungeon: "副本", trade: "交易", shop: "商店", guild: "行会", system: "系统" };
  modalTitle.textContent = titles[kind] || "";
  if (kind === "char") cmd("equipment");
  else if (kind === "bag") cmd("bag");
  else if (kind === "shop") cmd("shop");
  else if (kind === "quest") cmd("questlog");
  else if (kind === "party") cmd("partylist");
  else if (kind === "dungeon") cmd("dungeonlist");
  else if (kind === "guild") cmd("guildinfo");
  else if (kind === "skill") cmd("skills");
  renderPanel(kind);
}

function closePanel() {
  modal.classList.add("hidden");
  currentPanel = null;
}

function renderPanel(kind) {
  if (!S) return;
  if (kind === "char") {
    const eq = S.equip || [];
    const byPos = {};
    for (const it of eq) byPos[it.pos] = it;
    const slot = (pos, label, area) => {
      const it = byPos[pos];
      if (it) {
        return `<div class="doll-slot filled" style="grid-area:${area}"
          data-unequip="${pos}" data-id="${it.id}" title="${esc(it.name)}">
          <img src="${iconFor(it.name)}" alt=""><span>${esc(it.name)}</span></div>`;
      }
      return `<div class="doll-slot" style="grid-area:${area}"><span>${label}</span></div>`;
    };
    const pfn = playerMap[String(S.player_class)] || playerMap["0"];
    const portrait = pfn ? `<img src="/assets/thirdparty/dcss/${pfn}" alt="">` : "";
    modalBody.innerHTML = `
      <div class="doll">
        ${slot(2, "头盔", "1 / 2")}
        ${slot(3, "项链", "1 / 3")}
        ${slot(0, "武器", "2 / 1")}
        <div class="doll-center" style="grid-area:2 / 2 / 3 / 4">
          <div class="doll-portrait">${portrait}</div>
          <div class="doll-name">${esc(S.name)} · ${CLASS_NAME[S.player_class] || ""}</div>
        </div>
        ${slot(1, "衣服", "2 / 4")}
        ${slot(4, "戒指", "3 / 1")}
        ${slot(5, "戒指", "3 / 2")}
        ${slot(6, "手镯", "3 / 3")}
        ${slot(7, "手镯", "3 / 4")}
        ${slot(8, "腰带", "4 / 2")}
        ${slot(9, "靴子", "4 / 3")}
      </div>
      <div class="stats-grid">
        <div><span>等级</span><b>${S.level}</b></div>
        <div><span>职业</span><b>${CLASS_NAME[S.player_class] || ""}</b></div>
        <div><span>攻击</span><b>${S.min_atk}-${S.max_atk}</b></div>
        <div><span>防御</span><b>${S.min_def}-${S.max_def}</b></div>
        <div><span>魔法</span><b>${S.min_mc}-${S.max_mc}</b></div>
        <div><span>道术</span><b>${S.min_sc}-${S.max_sc}</b></div>
      </div>
      <div class="muted" style="text-align:center">点击装备可卸下</div>`;
  } else if (kind === "bag") {
    const bag = S.bag || [];
    const cells = bag.map(it => `
      <div class="bag-cell" data-bag="${it.id}" title="${esc(it.name)}${it.up ? " +" + it.up : ""} · 耐久 ${it.dura}/${it.maxdura}">
        <img src="${iconFor(it.name)}" alt="">
        <span>${esc(it.name)}</span>
        ${it.count > 1 ? `<b>${it.count}</b>` : ""}
      </div>`).join("");
    modalBody.innerHTML = `<div class="grid-bag">${cells || '<div class="muted">背包是空的</div>'}</div>
      <div class="muted" style="margin-top:8px">左键使用/装备，右键出售</div>`;
  } else if (kind === "skill") {
    const bag = S.bag || [];
    const rows = (S.skills || []).map((sk, i) => {
      const book = bag.find(it => it.name === sk.name + "书");
      let action;
      if (sk.learned) action = `<button data-cast="${esc(sk.name)}">施放</button>`;
      else if (S.level < sk.need) action = `<span class="muted">需 ${sk.need} 级</span>`;
      else if (book) action = `<button data-learn="${book.id}">学习</button>`;
      else action = '<span class="muted">需技能书</span>';
      return `<div class="row"><div class="grow">F${i + 1} ${esc(sk.name)}
        <div class="muted">需求 ${sk.need} 级 · MP ${sk.mp} · 冷却 ${(sk.cd / 1000).toFixed(1)}s</div></div>
        ${action}</div>`;
    }).join("");
    modalBody.innerHTML = rows +
      '<div class="muted" style="margin-top:8px">技能书可在杂货商购买，使用后消耗</div>';
  } else if (kind === "quest") {
    const rows = (S.quests || []).map(q => {
      const st = { 0: "可接", 1: "进行中", 2: "已完成" }[q.state] || "";
      let action = '<span class="muted">已完成</span>';
      if (q.state === 0) {
        action = `<button data-quest="accept" data-name="${esc(q.name)}">接取</button>`;
      } else if (q.state === 1) {
        const ready = q.progress >= q.need;
        action = `<button data-quest="submit" data-name="${esc(q.name)}" ${ready ? "" : "disabled"}>提交</button>`;
      }
      return `<div class="row"><div class="grow">[${st}] ${esc(q.name)}
        <div class="muted">${esc(q.target)} ${q.progress}/${q.need} · 奖励 ${q.exp} 经验 / ${q.gold} 金币</div>
        <div class="muted">发布：${esc(q.npc || "?")} · 需求等级 ${q.needlevel || 1}${q.reward ? " · 物品 " + esc(q.reward) + " x" + q.rewardcount : ""}</div>
        </div>${action}</div>`;
    }).join("");
    modalBody.innerHTML = (rows || '<div class="muted">没有任务数据</div>') +
      '<div class="muted" style="margin-top:8px">接取/提交需要靠近发布任务的 NPC</div>';
  } else if (kind === "party") {
    const members = S.party || [];
    const rows = members.map((m, i) => `
      <div class="member-row">
        <b>${i === 0 ? "队长 " : ""}${esc(m.name)}</b>
        <span class="muted">Lv.${m.level} · ${CLASS_NAME[m.cls] || ""}</span>
        <div class="bar"><i style="width:${Math.max(0, Math.min(100, m.hp / Math.max(1, m.maxhp) * 100))}%"></i></div>
        <span class="muted">${m.hp}/${m.maxhp}${m.online ? "" : " 离线"}</span>
      </div>`).join("");
    modalBody.innerHTML = (rows || '<div class="muted">你还没有队伍，邀请附近玩家加入。</div>') +
      `<div class="social-form"><input id="party-invite-name" placeholder="玩家名">
       <button data-party-invite>邀请</button>
       <button data-party-leave>离开队伍</button></div>`;
  } else if (kind === "dungeon") {
    const defs = S.dungeons || [];
    const rows = defs.map(d => `
      <div class="row"><div class="grow"><b>${esc(d.name)}</b>
        <div class="muted">Boss：${esc(d.boss)} · 实例地图 ${d.maps} 张 · ${d.first ? "已首通" : "未首通"}</div>
      </div></div>`).join("");
    const recs = (S.dungeon_records || []).map(r =>
      `<div class="row"><div class="grow">${esc(r.dungeon)} · ${esc(r.party)}
        <div class="muted">${r.seconds}s · ${esc(r.date)}</div></div></div>`).join("");
    const slots = (S.dungeon_slots || []).map(s =>
      `<div class="row"><div class="grow">地图 ${s.map} · ${esc(s.party)}</div></div>`).join("");
    modalBody.innerHTML = (rows || '<div class="muted">没有副本数据</div>') +
      '<div class="muted" style="margin-top:8px">请到各副本 NPC 处选择难度进入</div>' +
      (recs ? '<div class="qt-title" style="margin-top:8px;color:#5a2a10">排行榜</div>' + recs : '') +
      (slots ? '<div class="qt-title" style="margin-top:8px;color:#5a2a10">当前实例</div>' + slots : '');
  } else if (kind === "trade") {
    const t = S.trade;
    if (!t) {
      modalBody.innerHTML = `
        <div class="muted">输入附近玩家名发起交易，双方需在同一地图 6 格内。</div>
        <div class="social-form"><input id="trade-target" placeholder="玩家名">
          <button data-trade-open>发起交易</button></div>`;
    } else {
      const itemRows = (arr) => (arr || []).map(it =>
        `<div class="row"><div class="grow">${esc(it.name)} x${it.count}</div></div>`).join("");
      const bagRows = (S.bag || []).slice(0, 24).map(it =>
        `<button class="mini" data-trade-add="${it.id}">${esc(it.name)}${it.count > 1 ? " x" + it.count : ""}</button>`).join("");
      modalBody.innerHTML = `
        <div class="stats-grid">
          <div><span>对方</span><b>${esc(t.partner)}</b></div>
          <div><span>状态</span><b>${t.my_locked ? "已锁定" : "未锁定"} / ${t.other_locked ? "对方已锁" : "对方未锁"}</b></div>
        </div>
        <div class="qt-title" style="color:#5a2a10">我的物品</div>${itemRows(t.my_items) || '<div class="muted">空</div>'}
        <div class="qt-title" style="color:#5a2a10">对方物品</div>${itemRows(t.other_items) || '<div class="muted">空</div>'}
        <div class="social-form"><input id="trade-gold" type="number" value="${t.my_gold}">
          <button data-trade-gold>设置金币</button>
          <button data-trade-lock>锁定</button>
          <button data-trade-confirm>确认交易</button>
          <button data-trade-cancel>取消</button></div>
        <div class="muted" style="margin-top:6px">从背包放入：</div>
        <div class="social-form" style="flex-wrap:wrap">${bagRows || '<span class="muted">背包为空</span>'}</div>`;
    }
  } else if (kind === "shop") {
    const rows = (S.shop || []).map(it => `
      <div class="row"><img src="${iconFor(it.name)}" alt="">
        <div class="grow">${esc(it.name)}</div>
        <div class="muted">${it.price} 金币</div>
        <button data-buy="${esc(it.name)}">购买</button></div>`).join("");
    modalBody.innerHTML = rows || '<div class="muted">商店数据加载中…</div>';
  } else if (kind === "guild") {
    const g = S.guild;
    if (!g) {
      modalBody.innerHTML = `
        <div class="muted">尚未加入行会。创建需要 1000 金币。</div>
        <div class="social-form"><input id="guild-create-name" placeholder="新行会名">
          <button data-guild-create>创建</button></div>
        <div class="social-form"><input id="guild-join-name" placeholder="行会名">
          <button data-guild-join>加入</button></div>`;
    } else {
      const members = (g.members || []).map(m => `
        <div class="member-row"><b>${m.rank === 2 ? "会长 " : (m.rank === 1 ? "副会长 " : "")}${esc(m.name)}</b>
          <span class="muted">Lv.${m.level}</span><span class="muted">${m.online ? "在线" : "离线"}</span></div>`).join("");
      modalBody.innerHTML = `
        <div class="stats-grid">
          <div><span>行会</span><b>${esc(g.name)}</b></div>
          <div><span>等级</span><b>Lv.${g.level}</b></div>
          <div><span>资金</span><b>${g.funds}</b></div>
          <div><span>成员</span><b>${g.members.length}</b></div>
        </div>${members}
        <div class="social-form"><input id="guild-donate-amount" type="number" value="1000">
          <button data-guild-donate>捐献</button><button data-guild-leave>退会</button></div>`;
    }
  } else if (kind === "system") {
    modalBody.innerHTML = `
      <div class="stats-grid">
        <div><span>连接</span><b>${S.connected ? "已连接" : "未连接"}</b></div>
        <div><span>地图</span><b>${esc(S.title || S.map || "")}</b></div>
        <div><span>坐标</span><b>${S.x},${S.y}</b></div>
        <div><span>金币</span><b>${S.gold}</b></div>
      </div>
      <div class="muted" style="line-height:2">
        聊天框命令：/gmhelp  GM 命令<br>/quests  任务状态<br>/party  队伍<br>/guild list  行会列表
      </div>
      <div class="pass-box">
        <input id="old-pass" type="password" placeholder="旧密码">
        <input id="new-pass" type="password" placeholder="新密码（至少4位）">
        <button data-pass>修改密码</button>
      </div>`;
  }
}

document.querySelectorAll("#menu-buttons button").forEach(b => {
  b.onclick = () => openPanel(b.dataset.panel);
});
$("modal-close").onclick = closePanel;
modal.addEventListener("click", ev => { if (ev.target === modal) closePanel(); });

modalBody.addEventListener("click", ev => {
  const bag = ev.target.closest("[data-bag]");
  if (bag && S) {
    const it = (S.bag || []).find(x => x.id === Number(bag.dataset.bag));
    if (it) cmd(it.usetype ? "use " + it.id : "equipauto " + it.id);
    return;
  }
  const un = ev.target.closest("[data-unequip]");
  if (un) { cmd(`unequip ${un.dataset.unequip} ${un.dataset.id}`); return; }
  const buy = ev.target.closest("[data-buy]");
  if (buy) { cmd(`buy ${buy.dataset.buy} 1`); return; }
  const castBtn = ev.target.closest("[data-cast]");
  if (castBtn) { cmd("cast " + castBtn.dataset.cast); return; }
  const learn = ev.target.closest("[data-learn]");
  if (learn) {
    cmd("use " + learn.dataset.learn);
    setTimeout(() => { cmd("skills"); cmd("bag"); }, 400);
    return;
  }
  if (ev.target.closest("[data-trade-open]")) {
    const n = ($("trade-target") || {}).value || "";
    if (n) cmd("trade " + n.trim());
    return;
  }
  if (ev.target.closest("[data-trade-add]")) { cmd("trade add " + ev.target.closest("[data-trade-add]").dataset.tradeAdd); return; }
  if (ev.target.closest("[data-trade-gold]")) {
    const g = ($("trade-gold") || {}).value || "0";
    cmd("trade gold " + parseInt(g, 10));
    return;
  }
  if (ev.target.closest("[data-trade-lock]")) { cmd("trade lock"); return; }
  if (ev.target.closest("[data-trade-confirm]")) { cmd("trade confirm"); return; }
  if (ev.target.closest("[data-trade-cancel]")) { cmd("trade cancel"); return; }
  if (ev.target.closest("[data-party-invite]")) {
    const n = ($("party-invite-name") || {}).value || "";
    if (n) cmd("partyinvite " + n.trim());
    return;
  }
  if (ev.target.closest("[data-party-leave]")) { cmd("partyleave"); return; }
  if (ev.target.closest("[data-guild-create]")) {
    const n = ($("guild-create-name") || {}).value || "";
    if (n) cmd("guild create " + n.trim());
    return;
  }
  if (ev.target.closest("[data-guild-join]")) {
    const n = ($("guild-join-name") || {}).value || "";
    if (n) cmd("guild join " + n.trim());
    return;
  }
  if (ev.target.closest("[data-guild-leave]")) { cmd("guild leave"); return; }
  if (ev.target.closest("[data-guild-donate]")) {
    const n = ($("guild-donate-amount") || {}).value || "1000";
    cmd("guild donate " + parseInt(n, 10));
    return;
  }
  const passBtn = ev.target.closest("[data-pass]");
  if (passBtn) {
    const oldPass = ($("old-pass") || {}).value || "";
    const newPass = ($("new-pass") || {}).value || "";
    changePassword(oldPass, newPass);
    return;
  }
  const quest = ev.target.closest("[data-quest]");
  if (quest) {
    cmd(`${quest.dataset.quest} ${quest.dataset.name}`);
    setTimeout(() => cmd("questlog"), 400);
    return;
  }
});
modalBody.addEventListener("contextmenu", ev => {
  const bag = ev.target.closest("[data-bag]");
  if (bag) { ev.preventDefault(); cmd("sellitem " + bag.dataset.bag); }
});

async function refresh() {
  try {
    const r = await fetch("/state");
    S = await r.json();
    lastState = Date.now();
    if (lastLevel && S.level > lastLevel) {
      localFx.push({ x: S.x, y: S.y, text: "升级！", color: "#ffd76a", until: Date.now() + 1400 });
      sfx("level");
    }
    lastLevel = S.level;
    if ((S.effects || []).some(e => e.kind === "damage" && e.left > 0.65)) sfx("hit");
    updateHud();
    drawMinimapDOM();
    renderChat();
    renderSkills();
    renderDialog();
    renderQuestTracker();
    renderDeath();
    checkMapChange();
    renderCharacters();
    if (S.trade && currentPanel !== "trade") openPanel("trade");
    if (currentPanel) renderPanel(currentPanel);
  } catch (e) {}
  setTimeout(refresh, 180);
}

function walk(dir) { cmd("walk " + dir); }

function eventPoint(ev) {
  const r = canvas.getBoundingClientRect();
  return [(ev.clientX - r.left) * W / r.width, (ev.clientY - r.top) * H / r.height];
}

function eventTile(ev) {
  const [x, y] = eventPoint(ev);
  const a = (x - CX) / (TW / 2), b = (y - CY) / (TH / 2);
  return { x, y, tx: Math.round(S.x + (a + b) / 2), ty: Math.round(S.y + (b - a) / 2) };
}

function inHud(x, y) {
  return (x < 330 && y < 150) || (x > W - 205 && y < 150) || (y > H - 120);
}

function entityAt(tx, ty) {
  return (S ? S.entities : []).find(e => e.x === tx && e.y === ty);
}

function distTo(e) {
  return Math.max(Math.abs(e.x - S.x), Math.abs(e.y - S.y));
}

function dirTo(dx, dy) {
  const sx = Math.sign(dx), sy = Math.sign(dy);
  const table = { "0,-1": 0, "1,-1": 1, "1,0": 2, "1,1": 3, "0,1": 4, "-1,1": 5, "-1,0": 6, "-1,-1": 7 };
  return table[sx + "," + sy] ?? 4;
}

function stepToward(tx, ty, now) {
  if (!S || now - lastStep < 300) return;
  const dx = tx - S.x, dy = ty - S.y;
  if (dx === 0 && dy === 0) return;

  if (S.x === lastPos.x && S.y === lastPos.y) stuck++;
  else stuck = 0;
  lastPos = { x: S.x, y: S.y };

  let dir;
  if (Math.abs(dx) >= Math.abs(dy)) dir = dx > 0 ? 2 : 6;
  else dir = dy > 0 ? 4 : 0;

  if (stuck >= 2) {
    if (dy !== 0) dir = dy > 0 ? 4 : 0;
    else if (dx !== 0) dir = dx > 0 ? 2 : 6;
  }
  cmd("walk " + dir);
  lastStep = now;
}

function autoPilot() {
  if (!S) return;
  const now = Date.now();

  if (holding && holdGoal) moveGoal = { type: "move", x: holdGoal.x, y: holdGoal.y };

  if (targetId) {
    const t = S.entities.find(e => e.id === targetId);
    if (!t) { targetId = 0; return; }
    if (distTo(t) <= 1) {
      if (now - lastAttack > 900) {
        cmd("attack " + dirTo(t.x - S.x, t.y - S.y));
        lastAttack = now;
        selfAttackUntil = now + 260;
        sfx("attack");
      }
      return;
    }
    stepToward(t.x, t.y, now);
    return;
  }

  if (!moveGoal) return;
  if (moveGoal.type === "talk") {
    const n = S.entities.find(e => e.id === moveGoal.id);
    if (!n) { moveGoal = null; return; }
    if (distTo(n) <= 3) { cmd("talk " + n.name); moveGoal = null; return; }
    stepToward(n.x, n.y, now);
  } else if (moveGoal.type === "item") {
    if (S.x === moveGoal.x && S.y === moveGoal.y) { cmd("pickup"); moveGoal = null; return; }
    stepToward(moveGoal.x, moveGoal.y, now);
  } else {
    if (S.x === moveGoal.x && S.y === moveGoal.y) { moveGoal = null; return; }
    stepToward(moveGoal.x, moveGoal.y, now);
  }
}
setInterval(autoPilot, 100);

canvas.addEventListener("mousedown", ev => {
  if (ev.button !== 0 || !S) return;
  const p = eventTile(ev);
  if (inHud(p.x, p.y)) return;
  const ent = entityAt(p.tx, p.ty);
  targetId = 0;
  moveGoal = null;

  if (ent && ent.type === 3) {
    targetId = ent.id;
  } else if (ent && ent.type === 4) {
    moveGoal = { type: "talk", id: ent.id };
    if (distTo(ent) <= 3) { talkTo(ent.name); moveGoal = null; }
  } else if (ent && ent.type === 2) {
    moveGoal = { type: "item", x: p.tx, y: p.ty };
    if (S.x === p.tx && S.y === p.ty) { cmd("pickup"); moveGoal = null; }
  } else {
    moveGoal = { type: "move", x: p.tx, y: p.ty };
    holding = true;
    holdGoal = { x: p.tx, y: p.ty };
    sfx("step");
  }
});

canvas.addEventListener("mousemove", ev => {
  if (!S) return;
  const p = eventTile(ev);
  hoverTile = { x: p.tx, y: p.ty };
  if (holding) holdGoal = { x: p.tx, y: p.ty };
});

window.addEventListener("mouseup", () => { holding = false; holdGoal = null; });

canvas.addEventListener("contextmenu", ev => {
  ev.preventDefault();
  if (!S) return;
  const p = eventTile(ev);
  if (inHud(p.x, p.y)) return;
  const ent = entityAt(p.tx, p.ty);
  if (ent && ent.type === 3) {
    targetId = ent.id;
    return;
  }
  const near = S.entities.filter(e => e.type === 3 && distTo(e) === 1)[0];
  if (near) targetId = near.id;
  else if (ent && ent.type === 2 && p.tx === S.x && p.ty === S.y) cmd("pickup");
});

function talkNearest() {
  if (!S) return;
  let best = null, bd = 99;
  for (const e of S.entities) {
    if (e.type !== 4) continue;
    const d = distTo(e);
    if (d < bd) { bd = d; best = e; }
  }
  if (best) {
    targetId = 0;
    moveGoal = bd <= 3 ? null : { type: "talk", id: best.id };
    if (bd <= 3) talkTo(best.name);
  }
}

document.addEventListener("keydown", ev => {
  if (document.activeElement === inputEl) return;
  if (/^F([1-8])$/.test(ev.key) && S && S.skills) {
    ev.preventDefault();
    const sk = S.skills[parseInt(ev.key.slice(1), 10) - 1];
    if (sk) cast(sk);
    return;
  }
  if (/^[1-6]$/.test(ev.key)) {
    ev.preventDefault();
    usePotion(parseInt(ev.key, 10) - 1);
    return;
  }
  if (ev.key === "Tab") {
    ev.preventDefault();
    if (!S) return;
    const near = S.entities.filter(e => e.type === 3)
      .sort((a, b) => distTo(a) - distTo(b))[0];
    if (near) targetId = near.id;
    return;
  }
  if (ev.key === "Escape") {
    if (!modal.classList.contains("hidden")) closePanel();
    else { targetId = 0; moveGoal = null; }
    return;
  }
  const panelKey = { c: "char", b: "bag", k: "skill", q: "quest", p: "party", d: "dungeon", v: "shop", g: "guild" }[ev.key.toLowerCase()];
  if (panelKey && !ev.metaKey && !ev.ctrlKey && !ev.altKey) {
    ev.preventDefault(); openPanel(panelKey); return;
  }
  if (ev.key === "Enter") { inputEl.focus(); return; }
  if (ev.code === "Space") {
    ev.preventDefault();
    if (!S) return;
    const t = targetId ? S.entities.find(e => e.id === targetId) : null;
    cmd("attack " + (t ? dirTo(t.x - S.x, t.y - S.y) : S.dir));
    selfAttackUntil = Date.now() + 260;
    sfx("attack");
    return;
  }
  if (ev.key === "e" || ev.key === "E") { cmd("pickup"); return; }
  if (ev.key === "t" || ev.key === "T") { talkNearest(); return; }
});

audioToggle.onclick = toggleAudio;
try {
  if (localStorage.getItem("woool_audio") === "1") {
    audioOn = true; audioToggle.textContent = "音效：开";
  }
} catch (e) {}
document.addEventListener("pointerdown", () => {
  if (audioOn) { ensureAudio(); startBgm(); }
}, { once: true });

async function logout() {
  try { await fetch("/api/logout", { method: "POST" }); } catch (e) {}
  authToken = "";
  S = null;
  try { sessionStorage.removeItem("woool_token"); } catch (e) {}
  loginScreen.classList.remove("hidden");
  charScreen.classList.add("hidden");
  loginMsg.textContent = "已退出登录";
}

async function changePassword(oldPass, newPass) {
  if (!oldPass || !newPass || newPass.length < 4) {
    alert("请输入旧密码，新密码至少 4 位"); return;
  }
  try {
    const r = await fetch("/api/password", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Token": authToken },
      body: JSON.stringify({ old: oldPass, new: newPass }),
    });
    const d = await r.json();
    alert(d.ok ? "密码修改请求已发送" : (d.error || "修改失败"));
  } catch (e) { alert("修改失败"); }
}

function renderCharacters() {
  if (!S || !S.characters || charScreen.classList.contains("hidden")) return;
  if (S.name) { charScreen.classList.add("hidden"); return; }
  charList.innerHTML = "";
  if (!S.characters.length) {
    charList.innerHTML = '<div class="login-sub">还没有角色，先创建一个。</div>';
    return;
  }
  const names = ["战士", "法师", "道士"];
  S.characters.forEach(ch => {
    const d = document.createElement("div");
    d.className = "char-card";
    d.innerHTML = `<b>${esc(ch.name)}</b><span>${names[ch.cls] || ""} · Lv.${ch.level}</span>
      <button class="mini" data-del="${esc(ch.name)}">删</button>
      <button class="mini" data-ren="${esc(ch.name)}">改</button>`;
    d.onclick = (ev) => {
      if (ev.target.closest("[data-del]")) { deleteCharacter(ch.name); return; }
      if (ev.target.closest("[data-ren]")) { renameCharacter(ch.name); return; }
      selectCharacter(ch.name);
    };
    charList.appendChild(d);
  });
}

async function deleteCharacter(name) {
  if (!confirm("确定删除角色 " + name + " ？存档不可恢复。")) return;
  await fetch("/api/deletechar", { method: "POST", headers: {
    "Content-Type": "application/json", "X-Token": authToken },
    body: JSON.stringify({ name }) });
  refresh();
}

async function renameCharacter(name) {
  const newName = prompt("新角色名（2-20 字节）", name);
  if (!newName || newName === name) return;
  await fetch("/api/renamechar", { method: "POST", headers: {
    "Content-Type": "application/json", "X-Token": authToken },
    body: JSON.stringify({ old: name, new: newName }) });
  refresh();
}

async function selectCharacter(name) {
  charMsg.textContent = "正在进入游戏…";
  try {
    const r = await fetch("/api/select", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Token": authToken },
      body: JSON.stringify({ name }),
    });
    const d = await r.json();
    if (!d.ok) { charMsg.textContent = d.error || "进入失败"; return; }
    charScreen.classList.add("hidden");
    chatEl.innerHTML = "";
    chatCount = 0;
    lastMapLabel = "";
    refresh();
  } catch (e) { charMsg.textContent = "进入失败"; }
}

async function createCharacter() {
  const name = charName.value.trim();
  if (name.length < 2) { charMsg.textContent = "角色名至少 2 个字符"; return; }
  charMsg.textContent = "正在创建…";
  try {
    const r = await fetch("/api/create", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Token": authToken },
      body: JSON.stringify({ name, cls: charClass.value }),
    });
    const d = await r.json();
    if (!d.ok) { charMsg.textContent = d.error || "创建失败"; return; }
    charName.value = "";
    charMsg.textContent = "创建成功";
    refresh();
  } catch (e) { charMsg.textContent = "创建失败"; }
}

async function doAuth(register) {
  loginMsg.textContent = register ? "正在注册…" : "正在登录…";
  try {
    const r = await fetch(register ? "/api/register" : "/api/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        name: loginName.value.trim(),
        password: loginPass.value,
        cls: loginClass.value,
      }),
    });
    const d = await r.json();
    if (!d.ok) { loginMsg.textContent = d.error || "失败"; return; }
    authToken = d.token;
    try { sessionStorage.setItem("woool_token", authToken); } catch (e) {}
    loginScreen.classList.add("hidden");
    charAccount.textContent = loginName.value.trim();
    charScreen.classList.remove("hidden");
    chatEl.innerHTML = "";
    chatCount = 0;
    lastMapLabel = "";
    refresh();
  } catch (e) {
    loginMsg.textContent = "桥接服务不可用";
  }
}
$("login-btn").onclick = () => doAuth(false);
$("register-btn").onclick = () => doAuth(true);
loginPass.addEventListener("keydown", e => { if (e.key === "Enter") doAuth(false); });
logoutBtn.onclick = logout;
$("create-btn").onclick = createCharacter;
$("char-back").onclick = logout;

loadAssets();
refresh();
requestAnimationFrame(draw);

const bootPanel = new URLSearchParams(location.search).get("panel");
if (bootPanel) setTimeout(() => openPanel(bootPanel), 900);
