const S = {
  status: null,
  players: [],
  gm: [],
  dungeons: [],
  bosses: [],
  logs: [],
  lastLogId: 0,
  paused: false,
  filter: "",
  page: "overview",
  timer: null,
};

const PAGE_META = {
  overview: ["总览", "服务器实时状态与快捷操作"],
  players: ["在线玩家", "查看在线角色并执行传送、经验、金币、发物品、治疗、踢下线"],
  gm: ["GM 工具", "GM 权限管理、刷怪、治疗与全服公告"],
  dungeons: ["副本实例", "六个副本主题 × 三档难度，实时槽位占用"],
  bosses: ["Boss", "刷新点、坐标与存活倒计时"],
  logs: ["运行日志", "服务器控制台输出，支持过滤与暂停"],
  console: ["控制台", "直接向服务器发送管理命令"],
  settings: ["设置", "运行环境与服务控制"],
};

function $(id) { return document.getElementById(id); }

function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  }[c]));
}

async function api(path, opts) {
  const res = await fetch(path, opts);
  if (!res.ok) throw new Error("HTTP " + res.status);
  return res.json();
}

async function action(type, data = {}) {
  try {
    const r = await api("/api/action", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(Object.assign({ type }, data)),
    });
    toast(r.ok ? "已执行" : "执行失败", r.ok);
    setTimeout(refreshAll, 500);
  } catch (e) {
    toast("请求失败：" + e.message, false);
  }
}

let toastTimer = null;
function toast(text, ok = true) {
  const el = $("toast");
  el.textContent = text;
  el.style.borderColor = ok ? "rgba(76,195,138,.5)" : "rgba(239,107,107,.5)";
  el.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove("show"), 2200);
}

/* ------------------------------------------------------------------ nav */
function switchPage(page) {
  S.page = page;
  document.querySelectorAll(".page").forEach(p => p.classList.remove("active"));
  $("page-" + page).classList.add("active");
  document.querySelectorAll(".nav").forEach(b =>
    b.classList.toggle("active", b.dataset.page === page));
  const meta = PAGE_META[page] || ["", ""];
  $("page-title").textContent = meta[0];
  $("page-sub").textContent = meta[1];
  if (page === "settings") loadConfig();
  if (page === "logs") renderLogs();
}

document.querySelectorAll(".nav").forEach(btn =>
  btn.addEventListener("click", () => switchPage(btn.dataset.page)));
document.querySelectorAll("[data-goto]").forEach(a =>
  a.addEventListener("click", e => { e.preventDefault(); switchPage(a.dataset.goto); }));

/* ------------------------------------------------------------------ data */
async function refreshStatus() {
  try {
    const s = await api("/api/status");
    S.status = s;
    $("c-clients").textContent = s.clients;
    $("c-players").textContent = s.players.length;
    $("c-monsters").textContent = s.monsters;
    $("c-npc").textContent = s.npc;
    $("c-boss").textContent = s.boss;
    $("c-quests").textContent = s.quests;
    $("c-dungeon").textContent = s.dungeon_instances;
    $("c-season").textContent = s.season;

    const dot = $("side-dot");
    dot.className = "dot " + (s.running ? "on" : "off");
    $("side-text").textContent = s.running ? "服务器运行中" : "服务器已停止";
    const up = s.uptime || 0;
    $("side-uptime").textContent = s.running
      ? `运行 ${String(Math.floor(up / 3600)).padStart(2, "0")}:${String(Math.floor(up / 60) % 60).padStart(2, "0")}:${String(up % 60).padStart(2, "0")}`
      : "";
    drawTrend(s.trend || []);
  } catch (e) {
    $("side-dot").className = "dot off";
    $("side-text").textContent = "管理后台未连接";
  }
}

async function refreshPlayers() {
  try {
    const r = await api("/api/players");
    S.players = r.players || [];
    renderPlayers();
  } catch (e) {}
}

async function refreshDungeons() {
  try {
    const r = await api("/api/dungeons");
    S.dungeons = r.dungeons || [];
    renderDungeons();
  } catch (e) {}
}

async function refreshBosses() {
  try {
    const r = await api("/api/bosses");
    S.bosses = r.bosses || [];
    renderBosses();
  } catch (e) {}
}

async function refreshLogs() {
  if (S.paused) return;
  try {
    const r = await api("/api/logs?since=" + S.lastLogId);
    if (r.logs && r.logs.length) {
      S.logs.push(...r.logs);
      if (S.logs.length > 3000) S.logs.splice(0, S.logs.length - 3000);
      S.lastLogId = S.logs[S.logs.length - 1].id;
      renderLogs();
      renderMiniLog();
      renderConsole();
    }
  } catch (e) {}
}

async function loadConfig() {
  try {
    const c = await api("/api/config");
    const rows = [
      ["项目根目录", c.root],
      ["服务器程序", c.binary],
      ["游戏端口", c.port],
      ["地图数量", c.maps],
      ["Python", c.python],
    ];
    $("config-list").innerHTML = rows.map(([k, v]) =>
      `<dt>${esc(k)}</dt><dd>${esc(v)}</dd>`).join("");
  } catch (e) {}
}

/* ------------------------------------------------------------------ render */
function renderPlayers() {
  const q = ($("player-search").value || "").trim().toLowerCase();
  const body = $("players-body");
  const list = S.players.filter(p => !q || p.name.toLowerCase().includes(q));
  if (!list.length) {
    body.innerHTML = `<tr><td colspan="6" class="empty">暂无在线玩家</td></tr>`;
    return;
  }
  body.innerHTML = list.map(p => `
    <tr>
      <td><b>${esc(p.name)}</b></td>
      <td>${p.level}</td>
      <td>${p.map}</td>
      <td>${p.x}, ${p.y}</td>
      <td><span class="badge ${p.kind === "客户端" ? "green" : "gray"}">${esc(p.kind)}</span></td>
      <td class="right">
        <button class="btn ghost" data-act="teleport" data-name="${esc(p.name)}" data-map="${p.map}" data-x="${p.x}" data-y="${p.y}">传送</button>
        <button class="btn ghost" data-act="addexp" data-name="${esc(p.name)}">经验</button>
        <button class="btn ghost" data-act="addgold" data-name="${esc(p.name)}">金币</button>
        <button class="btn ghost" data-act="give" data-name="${esc(p.name)}">物品</button>
        <button class="btn ghost" data-act="heal" data-name="${esc(p.name)}">治疗</button>
        <button class="btn red" data-act="kick" data-name="${esc(p.name)}">踢下线</button>
      </td>
    </tr>`).join("");
}

function renderDungeons() {
  const box = $("dungeons");
  if (!S.dungeons.length) {
    box.innerHTML = `<div class="panel">暂无副本数据（点击左侧“在线玩家”刷新或等待采集）</div>`;
    return;
  }
  box.innerHTML = S.dungeons.map(d => {
    const slots = d.slots.map(s => `
      <div class="slot">
        <span class="idx">${s.index}</span>
        <span>${esc(s.difficulty)}</span>
        <span class="${s.state === "idle" ? "free" : "busy"}">
          ${s.state === "idle" ? "空闲" : "队伍 " + esc(s.party)}
        </span>
        <span class="meta">地图 ${s.map}${s.state === "busy" ? " · 剩余 " + s.remain + "s" : ""}</span>
      </div>`).join("");
    return `<div class="dungeon">
      <div class="panel-head">
        <h3>${esc(d.name)}</h3>
        <span class="sub">${d.total} 个实例</span>
      </div>
      <div class="slots">${slots}</div>
    </div>`;
  }).join("");
}

function renderBosses() {
  const body = $("boss-body");
  if (!S.bosses.length) {
    body.innerHTML = `<tr><td colspan="4" class="empty">暂无 Boss 数据</td></tr>`;
    return;
  }
  body.innerHTML = S.bosses.map(b => {
    let cls = "gray";
    if (b.status.includes("存活")) cls = "green";
    else if (b.status.includes("倒计时")) cls = "yellow";
    else if (b.status.includes("已击杀")) cls = "gray";
    return `<tr>
      <td><b>${esc(b.name)}</b></td>
      <td>${b.map}</td>
      <td>${b.x}, ${b.y}</td>
      <td><span class="badge ${cls}">${esc(b.status)}</span></td>
    </tr>`;
  }).join("");
}

function logHtml(logs) {
  return logs.map(l => {
    const text = esc(l.text);
    if (!S.filter || l.text.toLowerCase().includes(S.filter)) {
      return `<span class="log-line ${esc(l.tag)}"><span class="ts">${l.t}</span>${text}</span>`;
    }
    return "";
  }).join("");
}

function renderLogs() {
  const view = $("log-view");
  const nearBottom = view.scrollTop + view.clientHeight >= view.scrollHeight - 40;
  view.innerHTML = logHtml(S.logs.slice(-1200));
  if (nearBottom) view.scrollTop = view.scrollHeight;
}

function renderConsole() {
  const view = $("console-view");
  const nearBottom = view.scrollTop + view.clientHeight >= view.scrollHeight - 40;
  view.innerHTML = logHtml(S.logs.slice(-300));
  if (nearBottom) view.scrollTop = view.scrollHeight;
}

function renderMiniLog() {
  $("mini-log").innerHTML = logHtml(S.logs.slice(-8));
}

async function refreshGm() {
  try {
    const r = await api("/api/gm");
    S.gm = r.gm || [];
    renderGm();
  } catch (e) {}
}

function renderGm() {
  const body = $("gm-body");
  if (!S.gm.length) {
    body.innerHTML = `<tr><td colspan="3" class="empty">暂无 GM</td></tr>`;
    return;
  }
  body.innerHTML = S.gm.map(g => `
    <tr>
      <td><b>${esc(g.name)}</b></td>
      <td><span class="badge ${g.level >= 3 ? "yellow" : (g.level === 2 ? "green" : "gray")}">GM ${g.level}</span></td>
      <td class="right"><button class="btn red" data-gm-remove="${esc(g.name)}">移除</button></td>
    </tr>`).join("");
}

/* ------------------------------------------------------------------ trend */
function drawTrend(data) {
  const c = $("trend");
  const dpr = window.devicePixelRatio || 1;
  const w = c.clientWidth || 600;
  const h = 150;
  if (c.width !== w * dpr) {
    c.width = w * dpr;
    c.height = h * dpr;
  }
  const ctx = c.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);

  ctx.strokeStyle = "#1f2735";
  ctx.lineWidth = 1;
  for (let i = 1; i <= 3; i++) {
    const y = (h / 4) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }
  if (data.length < 2) return;

  const max = Math.max(3, ...data);
  const step = w / (data.length - 1);
  ctx.beginPath();
  data.forEach((v, i) => {
    const x = i * step;
    const y = h - 12 - (v / max) * (h - 30);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.strokeStyle = "#d9b36c";
  ctx.lineWidth = 2;
  ctx.stroke();

  ctx.lineTo(w, h);
  ctx.lineTo(0, h);
  ctx.closePath();
  const grad = ctx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, "rgba(217,179,108,.28)");
  grad.addColorStop(1, "rgba(217,179,108,0)");
  ctx.fillStyle = grad;
  ctx.fill();
}

/* ------------------------------------------------------------------ events */
$("btn-start").addEventListener("click", () => action("start"));
$("btn-stop").addEventListener("click", () => action("stop"));
$("btn-restart").addEventListener("click", () => action("restart"));
$("btn-client").addEventListener("click", () => action("client"));
$("btn-save").addEventListener("click", () => action("save"));
$("btn-refresh").addEventListener("click", refreshAll);
$("btn-announce").addEventListener("click", () => {
  const text = $("announce-input").value.trim();
  if (text) action("announce", { text });
});
$("btn-quick").addEventListener("click", () => {
  const cmd = $("quick-cmd").value.trim();
  if (cmd) action("command", { cmd });
});

$("btn-players-refresh").addEventListener("click", refreshPlayers);
$("player-search").addEventListener("input", renderPlayers);
$("btn-boss-refresh").addEventListener("click", refreshBosses);

$("log-search").addEventListener("input", e => { S.filter = e.target.value.trim().toLowerCase(); renderLogs(); });
$("log-pause").addEventListener("change", e => { S.paused = e.target.checked; });
$("btn-log-clear").addEventListener("click", () => { $("log-view").innerHTML = ""; $("mini-log").innerHTML = ""; });

$("btn-console").addEventListener("click", () => {
  const cmd = $("console-input").value.trim();
  if (cmd) {
    action("command", { cmd });
    $("console-input").value = "";
  }
});
$("console-input").addEventListener("keydown", e => {
  if (e.key === "Enter") $("btn-console").click();
});

$("s-start").addEventListener("click", () => action("start"));
$("s-stop").addEventListener("click", () => action("stop"));
$("s-restart").addEventListener("click", () => action("restart"));

document.addEventListener("click", e => {
  const b = e.target.closest("[data-act]");
  if (b) onPlayerAction(e);
});

function onPlayerAction(e) {
  const btn = e.target.closest("[data-act]");
  if (!btn) return;
  const name = btn.dataset.name;
  const act = btn.dataset.act;
  if (act === "teleport") {
    const map = prompt("目标地图 id", btn.dataset.map || "1");
    if (map === null) return;
    const x = prompt("目标 X", btn.dataset.x || "477");
    if (x === null) return;
    const y = prompt("目标 Y", btn.dataset.y || "222");
    if (y === null) return;
    action("teleport", { name, map, x, y });
  } else if (act === "addexp" || act === "addgold") {
    const amount = prompt(act === "addexp" ? "增加经验" : "增加金币", "1000");
    if (amount !== null) action(act, { name, amount });
  } else if (act === "heal") {
    action("command", { cmd: "heal " + name });
  } else if (act === "give") {
    const item = prompt("物品名（与 baseitem.txt 一致）", "金创药");
    if (item) action("give", { name, item });
  } else if (act === "kick") {
    if (confirm(`确定把 ${name} 踢下线？`)) action("kick", { name });
  }
}

/* ------------------------------------------------------------------ chips */
const QUICK = [
  ["服务器状态", "status"], ["在线玩家", "online"], ["Boss", "boss"],
  ["副本", "dungeon"], ["赛季", "season status"], ["首杀", "firstkills"],
  ["排行", "dungeonrank"], ["NPC", "npcs"], ["商店", "shop"], ["保存", "save"],
];
$("quick-chips").innerHTML = QUICK.map(([t, c]) =>
  `<span class="chip" data-cmd="${esc(c)}">${esc(t)}</span>`).join("");
$("quick-chips").addEventListener("click", e => {
  const chip = e.target.closest(".chip");
  if (chip) action("command", { cmd: chip.dataset.cmd });
});

$("btn-gm-add").addEventListener("click", () => {
  const name = $("gm-name").value.trim();
  if (!name) return;
  action("command", { cmd: `gmadd ${name} ${$("gm-level").value}` });
  $("gm-name").value = "";
});
$("btn-gm-list").addEventListener("click", refreshGm);
$("btn-gm-spawn").addEventListener("click", () => {
  const name = $("gm-spawn-name").value.trim();
  if (!name) return;
  action("command", {
    cmd: `spawn ${name} ${$("gm-spawn-map").value || 1} ${$("gm-spawn-x").value || 477} ${$("gm-spawn-y").value || 222}`
  });
});
$("btn-gm-heal").addEventListener("click", () => {
  const name = $("gm-heal-name").value.trim();
  if (name) action("command", { cmd: "heal " + name });
});
$("btn-gm-announce").addEventListener("click", () => {
  const text = $("gm-announce").value.trim();
  if (text) action("announce", { text });
});
document.addEventListener("click", e => {
  const b = e.target.closest("[data-gm-remove]");
  if (b && confirm(`移除 GM ${b.dataset.gmRemove}？`))
    action("command", { cmd: "gmremove " + b.dataset.gmRemove });
});

/* ------------------------------------------------------------------ boot */
async function refreshAll() {
  await Promise.all([refreshStatus(), refreshPlayers(), refreshDungeons(), refreshBosses(), refreshGm()]);
  $("last-refresh").textContent = "刷新于 " + new Date().toLocaleTimeString();
}

refreshAll();
loadConfig();
setInterval(refreshStatus, 2000);
setInterval(refreshPlayers, 3000);
setInterval(refreshDungeons, 4000);
setInterval(refreshBosses, 4000);
setInterval(refreshGm, 5000);
setInterval(refreshLogs, 1200);
