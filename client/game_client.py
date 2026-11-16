#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GameServer 图形客户端（tkinter）。

用法：
    python3 client/game_client.py
    python3 client/game_client.py --host 127.0.0.1 --port 7100
    python3 client/game_client.py --headless --name 客户端 --class 法师 --seconds 1.5

操作：
    方向键 / WASD   走路
    空格            攻击（面朝方向）
    E               拾取脚下物品
    T               与最近的 NPC 对话
    回车            聊天；以 / 开头发送服务端命令（如 /cast 火球术、/quests、/guild info）
"""
import argparse
import json
import math
import os
import socket
import zlib
import struct
import sys
import time

# ---------------------------------------------------------------------------
# 协议
# ---------------------------------------------------------------------------
SM = {
    0x0101: "SM_SETMAP", 0x0102: "SM_SETPLAYER", 0x0103: "SM_SETPLAYERNAME",
    0x0104: "SM_SETMAPNAME", 0x0105: "SM_READY", 0x0106: "SM_APPEAR",
    0x0107: "SM_DISAPPEAR", 0x0108: "SM_WALK", 0x0109: "SM_RUN",
    0x010A: "SM_TURN", 0x010B: "SM_ATTACK", 0x010C: "SM_BEATTACK",
    0x010D: "SM_CHAT", 0x010E: "SM_BACK", 0x010F: "SM_UPDATEPROP",
    0x0110: "SM_DOWNITEMAPPEAR", 0x0111: "SM_DOWNITEMDISAPPEAR",
    0x0112: "SM_ADDITEM", 0x0113: "SM_EQUIPRESULT", 0x0114: "SM_UNEQUIPRESULT",
    0x0115: "SM_ACTIONRESULT", 0x0116: "SM_DEATH", 0x0117: "SM_SPELL",
    0x0118: "SM_LEVELUP", 0x02C4: "SM_SETFIGHTMAP",
}

OBJ_DOWNITEM, OBJ_MONSTER, OBJ_NPC, OBJ_PLAYER = 2, 3, 4, 5
DIRS = {0: (0, -1), 1: (1, -1), 2: (1, 0), 3: (1, 1), 4: (0, 1), 5: (-1, 1), 6: (-1, 0), 7: (-1, -1)}


def decode(buf):
    return bytes(b ^ ((0x5A + i * 0x1D) & 0xFF) for i, b in enumerate(buf))


def make_frame(payload):
    return struct.pack("<I", len(payload)) + payload


# ---------------------------------------------------------------------------
# 音效：首次运行生成简易 WAV，macOS 用 afplay 异步播放
# ---------------------------------------------------------------------------
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")


def _write_tone(path, freqs, duration=0.08, volume=0.25, rate=22050):
    import wave
    import math as _m
    frames = bytearray()
    n = int(rate * duration)
    for i in range(n):
        t = i / float(rate)
        v = 0.0
        for f in freqs:
            v += _m.sin(2 * _m.pi * f * t)
        v = v / max(1, len(freqs)) * volume
        sample = int(max(-1.0, min(1.0, v)) * 32767)
        frames += struct.pack("<h", sample)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


def _png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def _write_png(path, w, h, pixels):
    raw = b"".join(b"\x00" + b"".join(bytes(px) for px in row) for row in pixels)
    data = (b"\x89PNG\r\n\x1a\n" +
            _png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            _png_chunk(b"IDAT", zlib.compress(raw, 9)) +
            _png_chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def _write_ppm(path, w, h, pixels):
    data = b"P6\n%d %d\n255\n" % (w, h)
    data += b"".join(bytes(px) for row in pixels for px in row)
    with open(path, "wb") as f:
        f.write(data)


def ensure_tiles(size=36):
    """生成草/石 PNG 图集（幂等），供 tkinter PhotoImage 使用。"""
    try:
        os.makedirs(ASSETS, exist_ok=True)

        def make_grass(phase):
            px = []
            for y in range(size):
                row = []
                for x in range(size):
                    n = (x * 13 + y * 7 + phase * 5) % 11
                    base = (47 + n, 74 + n, 51 + n)
                    row.append(base)
                px.append(row)
            # 草叶
            for i in range(18):
                bx = (i * 7 + phase * 3) % (size - 2) + 1
                by = (i * 11 + phase * 5) % (size - 4) + 3
                for k in range(3):
                    if 0 <= by - k < size:
                        px[by - k][bx] = (63, 107, 69)
            return px

        def make_stone():
            px = []
            for y in range(size):
                row = []
                for x in range(size):
                    n = (x * 5 + y * 3) % 7
                    base = (74 + n, 64 + n, 56 + n)
                    if y % 9 == 0 or (x + (y // 9) * 5) % 12 == 0:
                        base = (52, 45, 40)
                    row.append(base)
                px.append(row)
            return px

        specs = {
            "grass1.png": make_grass(0),
            "grass2.png": make_grass(1),
            "stone.png": make_stone(),
        }
        for name, pixels in specs.items():
            path = os.path.join(ASSETS, name)
            if not os.path.exists(path):
                _write_png(path, size, size, pixels)
            #	Tk 8.5 不支持 PNG，额外写 PPM 兼容缓存
            ppm = os.path.join(ASSETS, name.replace(".png", ".ppm"))
            if not os.path.exists(ppm):
                _write_ppm(ppm, size, size, pixels)
    except Exception:
        pass


def load_sprite_assets(tk):
    """加载 DCSS CC0 角色/怪物/物品 GIF；返回解析好的映射。"""
    base = os.path.join(ASSETS, "thirdparty", "dcss")
    try:
        with open(os.path.join(base, "sprites.json"), "r", encoding="utf-8") as f:
            man = json.load(f)
    except Exception:
        return None

    images = {}

    def img(fn):
        if fn not in images:
            try:
                images[fn] = tk.PhotoImage(file=os.path.join(base, fn))
            except Exception:
                images[fn] = None
        return images[fn]

    def zoom(fn):
        im = img(fn)
        if im is None:
            return None
        key = fn + "@2"
        if key not in images:
            try:
                images[key] = im.zoom(2)
            except Exception:
                images[key] = im
        return images[key]

    monsters = {name: zoom(fn) for name, fn in (man.get("monsters") or {}).items()}
    players = {int(k): zoom(fn) for k, fn in (man.get("players") or {}).items()}
    npcs = [zoom(fn) for fn in (man.get("npc") or [])]
    icons = {k: img(fn) for k, fn in (man.get("icons") or {}).items()}
    return {"manifest": man, "monsters": monsters, "players": players,
            "npcs": npcs, "icons": icons, "images": images}


def load_iso_assets(tk):
    """加载 CC0 等距瓦片；失败时返回空字典，客户端回退程序化绘制。"""
    base = os.path.join(ASSETS, "thirdparty", "stone_soup")
    try:
        with open(os.path.join(base, "manifest.json"), "r", encoding="utf-8") as f:
            man = json.load(f)
    except Exception:
        return {}
    tiles = {}
    for key, fn in (man.get("tiles") or {}).items():
        try:
            tiles[key] = tk.PhotoImage(file=os.path.join(base, fn))
        except Exception:
            pass
    return tiles


def ensure_sounds():
    try:
        os.makedirs(ASSETS, exist_ok=True)
        specs = {
            "hit.wav": ([220, 180], 0.06),
            "spell.wav": ([520, 780], 0.12),
            "level.wav": ([660, 880], 0.18),
        }
        for name, (freqs, dur) in specs.items():
            path = os.path.join(ASSETS, name)
            if not os.path.exists(path):
                _write_tone(path, freqs, dur)
    except Exception:
        pass


_last_sound = {"t": 0.0}


def play_sound(name):
    now = time.time()
    if now - _last_sound["t"] < 0.08:
        return
    _last_sound["t"] = now
    path = os.path.join(ASSETS, name)
    try:
        if sys.platform == "darwin":
            import subprocess
            subprocess.Popen(["afplay", path], stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
        else:
            print("\a", end="", flush=True)
    except Exception:
        pass


class GameState:
    def __init__(self):
        self.id = 0
        self.name = ""
        self.player_class = 0
        self.map_name = ""
        self.map_title = ""
        self.x = 0
        self.y = 0
        self.dir = 4
        self.hp = 0
        self.mp = 0
        self.maxhp = 1
        self.maxmp = 1
        self.level = 1
        self.exp = 0
        self.maxexp = 1
        self.gold = 0
        self.min_atk = 0; self.max_atk = 0
        self.min_def = 0; self.max_def = 0
        self.min_mc = 0; self.max_mc = 0
        self.min_sc = 0; self.max_sc = 0
        self.entities = {}
        self.chat = []
        self.bag = []
        self.equip = []
        self.shop = []
        self.skills = []
        self.quests = []
        self.titles = []
        self.available_points = 0
        self.codex = {"items": 0, "titles": 0, "ach": 0, "sets": []}
        self.cooldowns = {}
        self.terrain = None
        self.effects = []          # 飘字/法术特效
        self.dialog = None         # NPC 对话框 {npc,text,options}
        self.auth_token = ""       # 登录会话 token
        self.characters = []       # 账号下的角色列表
        self.party = []            # 队伍成员
        self.guild = None          # 行会信息
        self.dungeons = []         # 副本定义
        self.dungeon_records = []  # 副本排行
        self.dungeon_slots = []    # 当前实例
        self.trade = None          # 交易窗口
        self.death_at = 0.0        # 最近一次死亡时间
        self.flash = {}            # 实体受击闪白
        self.anim = 0              # 动画帧

    def add_chat(self, text):
        self.chat.append(text)
        if len(self.chat) > 200:
            del self.chat[:-200]


def _u8(d, o):
    return d[o], o + 1


def _u16(d, o):
    return struct.unpack_from("<H", d, o)[0], o + 2


def _u32(d, o):
    return struct.unpack_from("<I", d, o)[0], o + 4


def _str(d, o):
    n, o = _u8(d, o)
    return d[o:o + n].decode("utf-8", "replace"), o + n


class GameClient:
    def __init__(self, host, port, state):
        self.state = state
        self.host = host
        self.port = port
        self.sock = None
        self.recv_buf = b""
        self.connected = False

    def connect(self, name, cls="0", command=None):
        self.sock = socket.create_connection((self.host, self.port), timeout=3)
        self.sock.setblocking(False)
        self.connected = True
        self.send_text(command if command else ("enter %s %s" % (name, cls)))

    def send_text(self, text):
        if self.sock is None:
            return
        try:
            self.sock.sendall(make_frame(text.encode("utf-8")))
        except OSError:
            pass

    def poll(self):
        if self.sock is None or not self.connected:
            return 0
        try:
            chunk = self.sock.recv(65536)
        except BlockingIOError:
            return 0
        except OSError:
            self.connected = False
            return 0
        if not chunk:
            self.connected = False
            return 0
        self.recv_buf += chunk
        n = 0
        while len(self.recv_buf) >= 4:
            size = struct.unpack_from("<I", self.recv_buf, 0)[0]
            if size == 0 or size > 65536:
                self.recv_buf = b""
                break
            if len(self.recv_buf) < 4 + size:
                break
            payload = self.recv_buf[4:4 + size]
            self.recv_buf = self.recv_buf[4 + size:]
            self._on_packet(payload)
            n += 1
        return n

    def _on_packet(self, pkt):
        if not pkt or pkt[:1] != b"#" or len(pkt) < 18:
            return
        hdr = decode(pkt[1:17])
        flag, cmd = struct.unpack_from("<IH", hdr, 0)
        p0, p1, p2 = struct.unpack_from("<HHH", hdr, 6)
        payload = decode(pkt[17:-1]) if len(pkt) > 18 else b""
        self._apply(flag, cmd, p0, p1, p2, payload)

    def _apply(self, flag, cmd, p0, p1, p2, payload):
        st = self.state

        if cmd == 0x0101:                     # SETMAP
            st.id = flag
            st.x, st.y = p0, p1
            st.map_name = payload.decode("utf-8", "replace")
            st.entities.clear()
            st.terrain = None
            st.effects = []
            st.dialog = None
            st.flash.clear()
        elif cmd == 0x0102:                   # SETPLAYER
            st.id = flag
            st.x, st.y = p0, p1
        elif cmd == 0x0103:                   # SETPLAYERNAME
            st.name = payload.decode("utf-8", "replace")
        elif cmd == 0x0104:                   # SETMAPNAME
            st.map_title = payload.decode("utf-8", "replace")
        elif cmd == 0x0106:                   # APPEAR
            etype = (flag >> 24) & 0xFF
            name = ""
            hp = 0
            maxhp = 0
            if etype in (OBJ_PLAYER, OBJ_NPC):
                name = payload[:120].split(b"/")[0].decode("utf-8", "replace")
            elif etype == OBJ_MONSTER:
                if len(payload) >= 8:
                    hp = struct.unpack_from("<I", payload, 0)[0]
                    maxhp = struct.unpack_from("<I", payload, 4)[0]
                    name = payload[8:].decode("utf-8", "replace")
            else:
                name = payload.decode("utf-8", "replace")
            st.entities[flag] = {"id": flag, "type": etype, "name": name or "?", "x": p0, "y": p1,
                                 "hp": hp, "maxhp": maxhp if etype == OBJ_MONSTER else 0,
                                 "dir": p2}
        elif cmd == 0x0107:                   # DISAPPEAR
            st.entities.pop(flag, None)
        elif cmd in (0x0108, 0x0109):         # WALK / RUN
            ent = st.entities.get(flag)
            if ent:
                ent["x"], ent["y"], ent["dir"] = p0, p1, p2
            if flag == st.id:
                st.x, st.y, st.dir = p0, p1, p2
        elif cmd == 0x010B:                   # ATTACK
            ent = st.entities.get(flag)
            if ent:
                ent["dir"] = p2
            st.flash[flag] = time.time() + 0.2
            if flag == st.id:
                st.dir = p2
        elif cmd == 0x010C:                   # BEATTACK
            if flag == st.id:
                st.hp, st.mp = p0, p1
                st.add_chat("受到 %d 点伤害（HP %d）" % (p2, p0))
            ent = st.entities.get(flag)
            if ent:
                ent["hp"] = p0
                st.effects.append({"kind": "damage", "x": ent["x"], "y": ent["y"],
                                   "text": "-%d" % p2, "color": "#ff6666",
                                   "until": time.time() + 0.8})
            st.flash[flag] = time.time() + 0.25
            play_sound("hit.wav")
        elif cmd == 0x010D:                   # CHAT
            st.add_chat(payload.decode("utf-8", "replace"))
        elif cmd == 0x010F:                   # UPDATEPROP
            st.gold = flag
            if len(payload) >= 44:
                level, = struct.unpack_from("<H", payload, 0)
                exp, maxexp = struct.unpack_from("<II", payload, 4)
                _, hp, mp, _, maxhp, maxmp = struct.unpack_from("<6H", payload, 12)
                st.level, st.exp, st.maxexp = level, exp, maxexp
                st.hp, st.mp, st.maxhp, st.maxmp = hp, mp, maxhp, maxmp
                if len(payload) >= 38:
                    st.max_atk, st.min_atk = payload[26], payload[27]
                    st.max_def, st.min_def = payload[28], payload[29]
                    st.max_mc, st.min_mc = payload[30], payload[31]
                    st.max_def, st.min_def = payload[28], payload[29]
                    st.max_sc, st.min_sc = payload[34], payload[35]
        elif cmd == 0x0110:                   # DOWNITEMAPPEAR
            st.entities[flag] = {"id": flag, "type": OBJ_DOWNITEM,
                                 "name": payload.decode("utf-8", "replace") or "物品",
                                 "x": p0, "y": p1, "hp": 0, "dir": 0}
        elif cmd == 0x0111:                   # DOWNITEMDISAPPEAR
            st.entities.pop(flag, None)
        elif cmd == 0x0116:                   # DEATH
            st.hp = 0
            st.death_at = time.time()
            st.add_chat("你已经死亡，正在回城复活……")
        elif cmd == 0x0117:                   # SPELL
            caster = st.entities.get(flag)
            ex, ey = (caster["x"], caster["y"]) if caster else (st.x, st.y)
            st.effects.append({"kind": "spell", "x": ex, "y": ey, "text": "",
                               "color": "#7fd0ff", "until": time.time() + 0.5})
            play_sound("spell.wav")
        elif cmd == 0x0118:                   # LEVELUP
            st.add_chat("升级！当前等级 %d" % p0)
            play_sound("level.wav")
        elif cmd == 0x0119:                   # BAG
            o = 0
            n, o = _u16(payload, o)
            st.bag = []
            for _ in range(n):
                mid, o = _u32(payload, o)
                name, o = _str(payload, o)
                cnt, o = _u16(payload, o)
                up, o = _u8(payload, o)
                elem, o = _u8(payload, o)
                epow, o = _u8(payload, o)
                dura, o = _u16(payload, o)
                maxdura, o = _u16(payload, o)
                stdmode, o = _u8(payload, o)
                usetype, o = _u8(payload, o)
                st.bag.append({"id": mid, "name": name, "count": cnt, "up": up,
                               "elem": elem, "epow": epow,
                               "dura": dura, "maxdura": maxdura,
                               "stdmode": stdmode, "usetype": usetype})
        elif cmd == 0x011A:                   # EQUIP
            o = 0
            n, o = _u16(payload, o)
            st.equip = []
            for _ in range(n):
                pos, o = _u8(payload, o)
                mid, o = _u32(payload, o)
                name, o = _str(payload, o)
                up, o = _u8(payload, o)
                elem, o = _u8(payload, o)
                epow, o = _u8(payload, o)
                dura, o = _u16(payload, o)
                maxdura, o = _u16(payload, o)
                st.equip.append({"pos": pos, "id": mid, "name": name, "up": up,
                                 "elem": elem, "epow": epow,
                                 "dura": dura, "maxdura": maxdura})
        elif cmd == 0x011C:                   # SKILLS
            o = 0
            n, o = _u16(payload, o)
            st.skills = []
            for _ in range(n):
                sid, o = _u8(payload, o)
                name, o = _str(payload, o)
                need, o = _u16(payload, o)
                mp, o = _u16(payload, o)
                cd, o = _u32(payload, o)
                learned, o = _u8(payload, o)
                sktype, o = _u8(payload, o)
                rng, o = _u8(payload, o)
                st.skills.append({"id": sid, "name": name, "need": need, "mp": mp,
                                  "cd": cd, "learned": learned, "type": sktype, "range": rng})
        elif cmd == 0x011D:                   # QUESTS
            o = 0
            n, o = _u16(payload, o)
            st.quests = []
            for _ in range(n):
                qid, o = _u8(payload, o)
                name, o = _str(payload, o)
                target, o = _str(payload, o)
                need, o = _u16(payload, o)
                progress, o = _u16(payload, o)
                state, o = _u8(payload, o)
                qtype, o = _u8(payload, o)
                exp, o = _u32(payload, o)
                gold, o = _u32(payload, o)
                q = {"id": qid, "name": name, "target": target, "need": need,
                     "progress": progress, "state": state, "type": qtype,
                     "exp": exp, "gold": gold}
                if o < len(payload):
                    q["npc"], o = _str(payload, o)
                    q["needlevel"], o = _u8(payload, o)
                    q["reward"], o = _str(payload, o)
                    q["rewardcount"], o = _u16(payload, o)
                    q["prev"], o = _u8(payload, o)
                    q["dialog"], o = _str(payload, o)
                st.quests.append(q)
        elif cmd == 0x011E:                   # MAPDATA
            o = 0
            w, o = _u16(payload, o)
            h, o = _u16(payload, o)
            ox, o = _u16(payload, o)
            oy, o = _u16(payload, o)
            if ox > 32767:
                ox -= 65536
            if oy > 32767:
                oy -= 65536
            st.terrain = {"w": w, "h": h, "ox": ox, "oy": oy, "blocks": payload[o:o + w * h]}
        elif cmd == 0x0122:                   # AUTH
            st.auth_token = payload.decode("utf-8", "replace")
        elif cmd == 0x0124:                   # PARTY
            o = 0
            n, o = _u16(payload, o)
            st.party = []
            for _ in range(n):
                name, o = _str(payload, o)
                lvl, o = _u16(payload, o)
                cls, o = _u8(payload, o)
                hp, o = _u16(payload, o)
                maxhp, o = _u16(payload, o)
                online, o = _u8(payload, o)
                st.party.append({"name": name, "level": lvl, "cls": cls,
                                 "hp": hp, "maxhp": maxhp, "online": online})
        elif cmd == 0x0125:                   # GUILD
            o = 0
            name, o = _str(payload, o)
            if not name:
                st.guild = None
            else:
                leader, o = _str(payload, o)
                vice, o = _str(payload, o)
                level, o = _u8(payload, o)
                funds, o = _u32(payload, o)
                n, o = _u16(payload, o)
                members = []
                for _ in range(n):
                    mname, o = _str(payload, o)
                    mlvl, o = _u16(payload, o)
                    online, o = _u8(payload, o)
                    rank, o = _u8(payload, o)
                    members.append({"name": mname, "level": mlvl, "online": online, "rank": rank})
                wh, o = _u16(payload, o)
                st.guild = {"name": name, "leader": leader, "vice": vice,
                            "level": level, "funds": funds, "members": members, "warehouse": wh}
        elif cmd == 0x0127:                   # TRADE
            o = 0
            status, o = _u8(payload, o)
            if status == 0:
                st.trade = None
            else:
                partner, o = _str(payload, o)
                my_gold, o = _u32(payload, o)
                other_gold, o = _u32(payload, o)
                my_lock, o = _u8(payload, o)
                other_lock, o = _u8(payload, o)
                my_confirm, o = _u8(payload, o)
                other_confirm, o = _u8(payload, o)
                def items():
                    nonlocal o
                    n, o = _u16(payload, o)
                    arr = []
                    for _ in range(n):
                        mid, o = _u32(payload, o)
                        name, o = _str(payload, o)
                        cnt, o = _u16(payload, o)
                        arr.append({"id": mid, "name": name, "count": cnt})
                    return arr
                st.trade = {"partner": partner, "my_gold": my_gold, "other_gold": other_gold,
                            "my_locked": my_lock, "other_locked": other_lock,
                            "my_confirmed": my_confirm, "other_confirmed": other_confirm,
                            "my_items": items(), "other_items": items()}
        elif cmd == 0x0126:                   # DUNGEONS
            o = 0
            n, o = _u16(payload, o)
            st.dungeons = []
            for _ in range(n):
                did, o = _u8(payload, o)
                name, o = _str(payload, o)
                boss, o = _str(payload, o)
                first, o = _u8(payload, o)
                maps, o = _u8(payload, o)
                st.dungeons.append({"id": did, "name": name, "boss": boss,
                                    "first": first, "maps": maps})
            n, o = _u16(payload, o)
            st.dungeon_records = []
            for _ in range(n):
                dname, o = _str(payload, o)
                pname, o = _str(payload, o)
                secs, o = _u32(payload, o)
                date, o = _str(payload, o)
                st.dungeon_records.append({"dungeon": dname, "party": pname,
                                           "seconds": secs, "date": date})
            n, o = _u16(payload, o)
            st.dungeon_slots = []
            for _ in range(n):
                mid, o = _u16(payload, o)
                pname, o = _str(payload, o)
                st.dungeon_slots.append({"map": mid, "party": pname})
        elif cmd == 0x0123:                   # CHARLIST
            o = 0
            n, o = _u16(payload, o)
            st.characters = []
            for _ in range(n):
                name, o = _str(payload, o)
                cls, o = _u8(payload, o)
                lvl, o = _u16(payload, o)
                st.characters.append({"name": name, "cls": cls, "level": lvl})
        elif cmd == 0x0121:                   # DIALOG
            o = 0
            npc, o = _str(payload, o)
            text, o = _str(payload, o)
            n, o = _u16(payload, o)
            opts = []
            for _ in range(n):
                idx, o = _u8(payload, o)
                label, o = _str(payload, o)
                action, o = _str(payload, o)
                param, o = _str(payload, o)
                opts.append({"index": idx, "text": label, "action": action, "param": param})
            st.dialog = {"npc": npc, "text": text, "options": opts}
        elif cmd == 0x011F:                   # TITLES
            o = 0
            st.available_points, o = _u32(payload, o)
            n, o = _u16(payload, o)
            st.titles = []
            for _ in range(n):
                tid, o = _u16(payload, o)
                name, o = _str(payload, o)
                source, o = _u8(payload, o)
                cost, o = _u32(payload, o)
                owned, o = _u8(payload, o)
                active, o = _u8(payload, o)
                st.titles.append({"id": tid, "name": name, "source": source,
                                  "cost": cost, "owned": owned, "active": active})
        elif cmd == 0x0120:                   # CODEX
            o = 0
            items, o = _u32(payload, o)
            titles, o = _u32(payload, o)
            ach, o = _u32(payload, o)
            n, o = _u16(payload, o)
            sets = []
            for _ in range(n):
                name, o = _str(payload, o)
                found, o = _u8(payload, o)
                total, o = _u8(payload, o)
                complete, o = _u8(payload, o)
                sets.append({"name": name, "found": found, "total": total, "complete": complete})
            st.codex = {"items": items, "titles": titles, "ach": ach, "sets": sets}
        elif cmd == 0x011B:                   # SHOP
            o = 0
            n, o = _u16(payload, o)
            st.shop = []
            for _ in range(n):
                name, o = _str(payload, o)
                price, o = _u32(payload, o)
                st.shop.append({"name": name, "price": price})


# ---------------------------------------------------------------------------
# Headless（自动验收）
# ---------------------------------------------------------------------------
def run_headless(args):
    st = GameState()
    cli = GameClient(args.host, args.port, st)
    try:
        cli.connect(args.name, args.cls)
    except OSError as exc:
        print("连接失败: %s" % exc)
        return 1

    deadline = time.time() + args.seconds
    while time.time() < deadline:
        cli.poll()
        time.sleep(0.02)

    for cmd in [c for c in args.commands.split("|") if c.strip()]:
        cli.send_text(cmd.strip())
        end = time.time() + 0.25
        while time.time() < end:
            cli.poll()
            time.sleep(0.02)

    names = []
    for ent in st.entities.values():
        if ent["type"] == OBJ_NPC:
            names.append("NPC:" + ent["name"])
        elif ent["type"] == OBJ_MONSTER:
            names.append("MON:" + ent["name"])

    print("NAME=%s" % st.name)
    print("MAP=%s" % st.map_name)
    print("TITLE=%s" % st.map_title)
    print("POS=%d,%d" % (st.x, st.y))
    print("LEVEL=%d HP=%d/%d MP=%d/%d EXP=%d/%d GOLD=%d" % (
        st.level, st.hp, st.maxhp, st.mp, st.maxmp, st.exp, st.maxexp, st.gold))
    print("ENTITIES=%s" % ",".join(sorted(set(names))[:12]))
    if st.terrain:
        blocks = st.terrain["blocks"]
        print("TERRAIN=%dx%d BLOCKED=%d" % (
            st.terrain["w"], st.terrain["h"], sum(1 for b in blocks if b == 1)))
    print("BAG=%d EQUIP=%d SHOP=%d SKILLS=%d QUESTS=%d" % (
        len(st.bag), len(st.equip), len(st.shop), len(st.skills), len(st.quests)))
    if st.bag:
        print("BAG_ITEMS=%s" % ",".join("%s x%d" % (i["name"], i["count"]) for i in st.bag))
    print("TITLES=%d POINTS=%d" % (len(st.titles), st.available_points))
    print("CODEX=%d ITEMS=%d" % (len(st.codex["sets"]), st.codex["items"]))
    print("CHAT=%d" % len(st.chat))
    return 0


# ---------------------------------------------------------------------------
# tkinter 图形界面
# ---------------------------------------------------------------------------
def run_gui(args):
    ensure_sounds()
    ensure_tiles()
    import tkinter as tk
    from tkinter import ttk

    st = GameState()
    cli = GameClient(args.host, args.port, st)

    root = tk.Tk()
    root.title("传奇世界 · 重建服务端客户端")
    root.configure(bg="#1b1b1f")
    root.geometry("1040x830")

    iso_assets = load_iso_assets(tk)
    sprite_assets = load_sprite_assets(tk)

    def entity_sprite(ent):
        if sprite_assets is None:
            return None
        if ent["type"] == OBJ_MONSTER:
            name = ent["name"]
            return (sprite_assets["monsters"].get(name) or
                    sprite_assets["monsters"].get(name.split("(")[0]) or
                    sprite_assets["monsters"].get("半兽人"))
        if ent["type"] == OBJ_NPC:
            npcs = sprite_assets["npcs"]
            return npcs[sum(ord(c) for c in ent["name"]) % len(npcs)] if npcs else None
        if ent["type"] == OBJ_PLAYER:
            return sprite_assets["players"].get(st.player_class)
        return sprite_assets["icons"].get("misc")

    def self_sprite():
        if sprite_assets is None:
            return None
        return (sprite_assets["players"].get(st.player_class) or
                sprite_assets["players"].get(0))

    def item_icon(name):
        if sprite_assets is None:
            return None
        icons = sprite_assets["icons"]
        if "剑" in name or "刀" in name:
            key = "sword"
        elif "斧" in name:
            key = "axe"
        elif "杖" in name:
            key = "staff"
        elif "盔" in name:
            key = "helmet"
        elif "长袍" in name or "道袍" in name or "魔法长袍" in name or "道士服" in name:
            key = "robe"
        elif "甲" in name or "衣" in name or "服" in name:
            key = "armor"
        elif "项链" in name or "铃" in name:
            key = "amulet"
        elif "戒指" in name or "手镯" in name:
            key = "ring"
        elif "药" in name:
            key = "potion_hp" if "金创" in name else "potion_mp"
        elif "书" in name:
            key = "book"
        elif "金币" in name:
            key = "gold"
        else:
            key = "misc"
        return icons.get(key) or icons.get("misc")

    tiles = {}
    for key in ("grass1", "grass2", "stone"):
        for ext in (".png", ".ppm"):
            try:
                tiles[key] = tk.PhotoImage(file=os.path.join(ASSETS, key + ext))
                break
            except Exception:
                continue

    TILE = 64
    TILE_W, TILE_H = 64, 32
    VIEW_W, VIEW_H = 1024, 560
    COLS, ROWS = 16, 10

    login = tk.Frame(root, bg="#1b1b1f")
    login.pack(expand=True, fill="both")

    tk.Label(login, text="进入游戏", fg="#e8c37a", bg="#1b1b1f",
             font=("PingFang SC", 26, "bold")).pack(pady=(120, 20))

    row = tk.Frame(login, bg="#1b1b1f")
    row.pack()
    tk.Label(row, text="角色名", fg="#cccccc", bg="#1b1b1f").grid(row=0, column=0, padx=6)
    name_var = tk.StringVar(value="测试玩家")
    tk.Entry(row, textvariable=name_var, width=16).grid(row=0, column=1, padx=6)

    class_var = tk.StringVar(value="战士")
    tk.Label(row, text="职业", fg="#cccccc", bg="#1b1b1f").grid(row=0, column=2, padx=6)
    ttk.Combobox(row, textvariable=class_var, values=["战士", "法师", "道士"],
                 width=8, state="readonly").grid(row=0, column=3, padx=6)

    status_var = tk.StringVar(value="")
    tk.Label(login, textvariable=status_var, fg="#ff7777", bg="#1b1b1f").pack(pady=10)

    game = tk.Frame(root, bg="#1b1b1f")

    canvas = tk.Canvas(game, width=VIEW_W, height=VIEW_H,
                       bg="#0d0b08", highlightthickness=0)
    canvas.pack(pady=4)

    skillbar = tk.Frame(game, bg="#1b1b1f")
    skillbar.pack(pady=(0, 2))

    bar = tk.Canvas(game, width=COLS * TILE, height=34, bg="#101014", highlightthickness=0)
    bar.pack()

    log = tk.Text(game, height=6, bg="#141418", fg="#c8c8c8", insertbackground="#c8c8c8",
                  relief="flat", font=("Menlo", 11))
    log.pack(fill="x", padx=8, pady=(6, 2))
    log.configure(state="disabled")

    entry_row = tk.Frame(game, bg="#1b1b1f")
    entry_row.pack(fill="x", padx=8, pady=(0, 8))
    cmd_var = tk.StringVar()
    entry = tk.Entry(entry_row, textvariable=cmd_var, bg="#222228", fg="#eeeeee",
                     insertbackground="#eeeeee", relief="flat")
    entry.pack(side="left", fill="x", expand=True, ipady=4)

    shown = {"chat": 0}

    def log_append(text):
        log.configure(state="normal")
        log.insert("end", text + "\n")
        log.see("end")
        log.configure(state="disabled")

    def send_command(text):
        if not text:
            return
        if text.startswith("/"):
            cli.send_text(text[1:])
        else:
            cli.send_text("say " + text)

    def on_enter(_event=None):
        send_command(cmd_var.get().strip())
        cmd_var.set("")
        entry.focus_set()
        return "break"

    entry.bind("<Return>", on_enter)

    def walk(direction):
        cli.send_text("walk %d" % direction)
        st.dir = direction

    bindings = {
        "<Up>": 0, "<w>": 0, "<W>": 0,
        "<Right>": 2, "<d>": 2, "<D>": 2,
        "<Down>": 4, "<s>": 4, "<S>": 4,
        "<Left>": 6, "<a>": 6, "<A>": 6,
        "<KP_1>": 5, "<KP_3>": 3, "<KP_7>": 7, "<KP_9>": 1,
    }
    for seq, direction in bindings.items():
        root.bind(seq, lambda e, d=direction: walk(d))
    root.bind("<space>", lambda e: cli.send_text("attack %d" % st.dir))
    root.bind("<e>", lambda e: cli.send_text("pickup"))
    root.bind("<E>", lambda e: cli.send_text("pickup"))

    def talk_nearest():
        best = None
        best_d = 99
        for ent in st.entities.values():
            if ent["type"] != OBJ_NPC:
                continue
            d = max(abs(ent["x"] - st.x), abs(ent["y"] - st.y))
            if d < best_d:
                best, best_d = ent, d
        if best is not None:
            cli.send_text("talk " + best["name"])
        else:
            st.add_chat("附近没有 NPC")

    root.bind("<t>", lambda e: talk_nearest())
    root.bind("<T>", lambda e: talk_nearest())

    def draw_minimap():
        minimap.delete("all")
        t = st.terrain
        if t is None:
            minimap.create_text(90, 60, text="地形加载中…", fill="#666666")
            return

        mw, mh = 180, 120
        scale = min(mw / float(t["w"]), mh / float(t["h"]))
        ox = (mw - t["w"] * scale) / 2.0
        oy = (mh - t["h"] * scale) / 2.0
        blocks = t["blocks"]

        for y in range(t["h"]):
            for x in range(t["w"]):
                i = y * t["w"] + x
                v = blocks[i] if i < len(blocks) else 2
                if v == 2:
                    continue
                color = "#4a4038" if v == 1 else "#2f4a33"
                minimap.create_rectangle(ox + x * scale, oy + y * scale,
                                         ox + (x + 1) * scale, oy + (y + 1) * scale,
                                         fill=color, outline="")

        def dot(wx, wy, color, r=2):
            px = ox + (wx - t["ox"]) * scale
            py = oy + (wy - t["oy"]) * scale
            minimap.create_oval(px - r, py - r, px + r, py + r, fill=color, outline="")

        for ent in st.entities.values():
            if ent["type"] == OBJ_MONSTER:
                dot(ent["x"], ent["y"], "#d65a3f", 1.5)
            elif ent["type"] == OBJ_NPC:
                dot(ent["x"], ent["y"], "#e8c37a", 2)
            elif ent["type"] == OBJ_PLAYER:
                dot(ent["x"], ent["y"], "#3fa7d6", 2)
        dot(st.x, st.y, "#ffffff", 3)
        minimap.create_rectangle(0, 0, mw - 1, mh - 1, outline="#33333a")

    def draw_bars():
        bar.delete("all")
        w = COLS * TILE
        def draw(y, label, cur, maximum, color):
            ratio = 0 if maximum <= 0 else max(0.0, min(1.0, cur / float(maximum)))
            bar.create_rectangle(0, y, w, y + 12, fill="#26262c", outline="")
            bar.create_rectangle(0, y, int(w * ratio), y + 12, fill=color, outline="")
            bar.create_text(4, y + 6, anchor="w", fill="#ffffff", font=("Menlo", 9),
                            text="%s %d/%d" % (label, cur, maximum))
        draw(0, "HP", st.hp, st.maxhp, "#b03030")
        draw(14, "MP", st.mp, st.maxmp, "#3050b0")
        bar.create_text(w - 6, 6, anchor="e", fill="#e8c37a", font=("Menlo", 10),
                        text="EXP %d/%d   金币 %d" % (st.exp, st.maxexp, st.gold))

    def terrain_at(wx, wy):
        t = st.terrain
        if t is None:
            return 0
        x = wx - t["ox"]
        y = wy - t["oy"]
        if x < 0 or y < 0 or x >= t["w"] or y >= t["h"]:
            return 2
        i = y * t["w"] + x
        if i >= len(t["blocks"]):
            return 2
        return t["blocks"][i]

    def iso_pt(wx, wy):
        cx, cy = VIEW_W // 2, VIEW_H // 2 + 24
        dx, dy = wx - st.x, wy - st.y
        return cx + (dx - dy) * TILE_W / 2.0, cy + (dx + dy) * TILE_H / 2.0

    def hbar(x, y, w, h, ratio, color, back="#241a10"):
        ratio = max(0.0, min(1.0, ratio))
        canvas.create_rectangle(x, y, x + w, y + h, fill=back, outline="#5d4a2a")
        canvas.create_rectangle(x + 1, y + 1, x + 1 + int((w - 2) * ratio), y + h - 1,
                                fill=color, outline="")

    def draw_hud():
        #	左上角色面板
        canvas.create_rectangle(12, 12, 330, 142, fill="#120f0a", outline="#a8843f", width=2)
        canvas.create_rectangle(16, 16, 326, 138, outline="#3d2f18")
        canvas.create_oval(28, 28, 88, 88, fill="#2a2014", outline="#c9a24a", width=2)
        canvas.create_oval(38, 38, 78, 78, fill="#3f5f8a", outline="#1c2636")
        canvas.create_text(58, 58, text="战" if st.level else "?", fill="#f0d9a0",
                           font=("Songti SC", 16, "bold"))
        canvas.create_text(100, 26, anchor="w", text=st.name or "无名",
                           fill="#f0d9a0", font=("Songti SC", 14, "bold"))
        canvas.create_text(100, 46, anchor="w", text="Lv.%d  %s" % (st.level, st.map_title or st.map_name or ""),
                           fill="#c9a24a", font=("Songti SC", 10))
        hbar(100, 60, 210, 11, st.hp / float(max(1, st.maxhp)), "#a33131")
        canvas.create_text(206, 65, text="%d/%d" % (st.hp, st.maxhp), fill="#ffffff",
                           font=("Menlo", 8))
        hbar(100, 78, 210, 11, st.mp / float(max(1, st.maxmp)), "#2f4f9e")
        canvas.create_text(206, 83, text="%d/%d" % (st.mp, st.maxmp), fill="#ffffff",
                           font=("Menlo", 8))
        hbar(100, 96, 210, 9, st.exp / float(max(1, st.maxexp)), "#c9a24a")
        canvas.create_text(206, 101, text="EXP %d/%d" % (st.exp, st.maxexp), fill="#c8b88a",
                           font=("Menlo", 7))
        canvas.create_text(100, 120, anchor="w", text="金币 %d    坐标 %d,%d" % (st.gold, st.x, st.y),
                           fill="#d9b36c", font=("Songti SC", 9))

        #	右上小地图
        mx, my, mw, mh = VIEW_W - 196, 12, 184, 128
        canvas.create_rectangle(mx, my, mx + mw, my + mh, fill="#120f0a",
                                outline="#a8843f", width=2)
        t = st.terrain
        if t is not None:
            scale = min((mw - 12) / float(t["w"]), (mh - 12) / float(t["h"]))
            ox = mx + (mw - t["w"] * scale) / 2
            oy = my + (mh - t["h"] * scale) / 2
            for yy in range(0, t["h"], 2):
                for xx in range(0, t["w"], 2):
                    i = yy * t["w"] + xx
                    v = t["blocks"][i] if i < len(t["blocks"]) else 2
                    if v == 2:
                        continue
                    col = "#4a4038" if v == 1 else "#33513a"
                    canvas.create_rectangle(ox + xx * scale, oy + yy * scale,
                                            ox + (xx + 2) * scale, oy + (yy + 2) * scale,
                                            fill=col, outline="")
            for ent in st.entities.values():
                ex = ox + (ent["x"] - t["ox"]) * scale
                ey = oy + (ent["y"] - t["oy"]) * scale
                col = "#d65a3f" if ent["type"] == OBJ_MONSTER else (
                    "#e8c37a" if ent["type"] == OBJ_NPC else "#5aa9e6")
                canvas.create_oval(ex - 1, ey - 1, ex + 1, ey + 1, fill=col, outline="")
            px = ox + (st.x - t["ox"]) * scale
            py = oy + (st.y - t["oy"]) * scale
            canvas.create_oval(px - 3, py - 3, px + 3, py + 3, fill="#ffffff", outline="#000000")
        canvas.create_text(mx + 8, my + 8, anchor="nw", text="中州",
                           fill="#a8843f", font=("Songti SC", 9, "bold"))

    def draw_map():
        canvas.delete("all")
        cx, cy = VIEW_W // 2, VIEW_H // 2 + 24

        #	等距地表
        for s_ in range(-20, 21):
            for d in range(-20, 21):
                wx, wy = st.x + d, st.y + s_
                sx = cx + (d - s_) * TILE_W / 2.0
                sy = cy + (d + s_) * TILE_H / 2.0
                if sx < -TILE_W or sx > VIEW_W + TILE_W or sy < -TILE_H or sy > VIEW_H + TILE_H:
                    continue
                block = terrain_at(wx, wy)
                title = st.map_title or st.map_name or ""
                dungeon_like = any(k in title for k in ("殿", "谷", "寺", "坟", "洞", "地", "刹", "矿"))
                floor_keys = ["crypt", "crypt2", "crypt3"] if dungeon_like else ["grass", "grass2", "grass3"]
                floor_key = floor_keys[(wx * 7 + wy * 13) % len(floor_keys)]
                floor_img = iso_assets.get(floor_key)
                if floor_img is None:
                    floor_img = iso_assets.get("crypt" if dungeon_like else "grass")
                if block == 2:
                    base = "#17130d"
                elif block == 1:
                    base = "#4a4038" if (wx + wy) % 2 else "#40372f"
                else:
                    base = "#2f4a33" if (wx * 7 + wy * 13) % 3 else "#35543b"

                if floor_img is not None and block != 2:
                    canvas.create_image(sx, sy, image=floor_img, anchor="center")
                else:
                    pts = [(sx, sy - TILE_H / 2), (sx + TILE_W / 2, sy),
                           (sx, sy + TILE_H / 2), (sx - TILE_W / 2, sy)]
                    canvas.create_polygon(pts, fill=base, outline="#20170f")
                    if block == 0:
                        canvas.create_line(sx - 8, sy + 2, sx - 6, sy - 2, fill="#3f6b45")
                    elif block == 1:
                        canvas.create_line(sx - 10, sy, sx + 10, sy, fill="#332b26")

                if block == 1 and iso_assets.get("wall") is not None:
                    wall_keys = [k for k in ("wall", "wall2") if k in iso_assets]
                    wall_img = iso_assets[wall_keys[(wx * 3 + wy * 5) % len(wall_keys)]]
                    canvas.create_image(sx, sy + 16, image=wall_img, anchor="s")

        #	实体按深度排序
        ents = sorted(st.entities.values(), key=lambda e: (e["x"] - st.x) + (e["y"] - st.y))
        for ent in ents:
            sx, sy = iso_pt(ent["x"], ent["y"])
            if sx < -60 or sx > VIEW_W + 60 or sy < -60 or sy > VIEW_H + 60:
                continue
            flashed = st.flash.get(ent.get("id", 0), 0) > time.time()
            canvas.create_oval(sx - 14, sy + 6, sx + 14, sy + 13, fill="#1a140d", outline="")
            spr = entity_sprite(ent)
            if spr is not None:
                canvas.create_image(sx, sy + 12, image=spr, anchor="s")
                if flashed:
                    canvas.create_oval(sx - 14, sy - 26, sx + 14, sy + 8,
                                       outline="#ffffff", width=2)
            elif ent["type"] == OBJ_MONSTER:
                body = "#ffffff" if flashed else "#a8432f"
                canvas.create_oval(sx - 10, sy - 8, sx + 10, sy + 8, fill=body, outline="#3a1710")
                canvas.create_oval(sx - 6, sy - 16, sx + 6, sy - 4, fill=body, outline="#3a1710")
                canvas.create_oval(sx - 4, sy - 12, sx - 1, sy - 9, fill="#140d08")
                canvas.create_oval(sx + 1, sy - 12, sx + 4, sy - 9, fill="#140d08")
            elif ent["type"] == OBJ_NPC:
                canvas.create_polygon(sx, sy - 26, sx - 11, sy + 10, sx + 11, sy + 10,
                                      fill="#c9a24a" if not flashed else "#ffffff",
                                      outline="#5d4a2a")
                canvas.create_oval(sx - 6, sy - 30, sx + 6, sy - 18, fill="#f0c8a0", outline="")
            elif ent["type"] == OBJ_PLAYER:
                canvas.create_oval(sx - 6, sy - 28, sx + 6, sy - 16, fill="#f0c8a0", outline="")
                canvas.create_rectangle(sx - 8, sy - 16, sx + 8, sy + 8,
                                        fill="#3f6f9a" if not flashed else "#ffffff", outline="")
                canvas.create_line(sx + 8, sy - 10, sx + 16, sy - 22, fill="#d8d8e0", width=2)
            else:
                canvas.create_oval(sx - 6, sy - 6, sx + 6, sy + 8, fill="#4fb06d", outline="#1d3a26")

            if ent["type"] == OBJ_MONSTER and ent.get("maxhp", 0) > 0:
                ratio = max(0.0, min(1.0, ent.get("hp", 0) / float(ent["maxhp"])))
                canvas.create_rectangle(sx - 14, sy - 34, sx + 14, sy - 30,
                                        fill="#2a0c08", outline="")
                canvas.create_rectangle(sx - 14, sy - 34, sx - 14 + 28 * ratio, sy - 30,
                                        fill="#d65a3f", outline="")

            canvas.create_text(sx, sy + 18, text=ent["name"][:6], fill="#e8dcc0",
                               font=("Songti SC", 8))

        #	自己
        sx, sy = iso_pt(st.x, st.y)
        canvas.create_oval(sx - 16, sy + 6, sx + 16, sy + 14, fill="#1a140d", outline="")
        own = self_sprite()
        if own is not None:
            canvas.create_image(sx, sy + 12, image=own, anchor="s")
        else:
            canvas.create_oval(sx - 7, sy - 30, sx + 7, sy - 16, fill="#f0c8a0", outline="")
            canvas.create_rectangle(sx - 9, sy - 16, sx + 9, sy + 9, fill="#3f6f9a", outline="#1c2636")
            canvas.create_line(sx + 9, sy - 10, sx + 18, sy - 24, fill="#e8e8f0", width=2)
        canvas.create_text(sx, sy + 20, text=st.name[:8], fill="#f0d9a0",
                           font=("Songti SC", 9, "bold"))

        #	飘字/法术
        now = time.time()
        st.effects = [e for e in st.effects if e["until"] > now]
        for e in st.effects:
            ex, ey = iso_pt(e["x"], e["y"])
            life = max(0.0, min(1.0, (e["until"] - now) / 0.8))
            if e["kind"] == "damage":
                canvas.create_text(ex, ey - 30 - (1.0 - life) * 24, text=e["text"],
                                   fill=e["color"], font=("Songti SC", 11, "bold"))
            else:
                r = 12 + (1.0 - life) * 30
                canvas.create_oval(ex - r, ey - r, ex + r, ey + r, outline=e["color"], width=2)

        draw_hud()

    skill_buttons = []
    skill_shown = {"n": -1}

    def dir_toward(dx, dy):
        sx = (dx > 0) - (dx < 0)
        sy = (dy > 0) - (dy < 0)
        table = {(0, -1): 0, (1, -1): 1, (1, 0): 2, (1, 1): 3,
                 (0, 1): 4, (-1, 1): 5, (-1, 0): 6, (-1, -1): 7}
        return table.get((sx, sy), 4)

    def cast_skill(sk):
        if not sk["learned"] or st.level < sk["need"]:
            return
        cli.send_text("cast " + sk["name"])
        st.cooldowns[sk["id"]] = time.time() + sk["cd"] / 1000.0

    def refresh_skillbar():
        if len(st.skills) != skill_shown["n"]:
            skill_shown["n"] = len(st.skills)
            for w in skillbar.winfo_children():
                w.destroy()
            skill_buttons.clear()
            for idx, sk in enumerate(st.skills[:8]):
                b = tk.Button(skillbar, text=sk["name"], width=13, relief="flat",
                              bg="#2a2014", fg="#e8dcc0", font=("Songti SC", 10),
                              activebackground="#3d2f18",
                              command=lambda s=sk: cast_skill(s))
                b.pack(side="left", padx=2, pady=2)
                skill_buttons.append((b, sk))

        now = time.time()
        for idx, (btn, sk) in enumerate(skill_buttons):
            remain = st.cooldowns.get(sk["id"], 0) - now
            usable = bool(sk["learned"]) and st.level >= sk["need"] and st.mp >= sk["mp"] and remain <= 0
            text = "%d.%s" % (idx + 1, sk["name"])
            if remain > 0:
                text += " %.1fs" % remain
            elif not sk["learned"]:
                text += "（未学）"
            btn.configure(text=text, state="normal" if usable else "disabled",
                          bg="#3d2f18" if usable else "#201a12")

    def on_canvas_click(event):
        cx, cy = VIEW_W // 2, VIEW_H // 2 + 24
        a = (event.x - cx) / (TILE_W / 2.0)
        b = (event.y - cy) / (TILE_H / 2.0)
        dx = (a + b) / 2.0
        dy = (b - a) / 2.0
        tx, ty = int(round(st.x + dx)), int(round(st.y + dy))

        target = None
        for ent in st.entities.values():
            if ent["x"] == tx and ent["y"] == ty and ent["type"] in (OBJ_NPC, OBJ_MONSTER, OBJ_DOWNITEM):
                target = ent
                break
        if target is None:
            return

        d = max(abs(tx - st.x), abs(ty - st.y))
        if target["type"] == OBJ_NPC:
            if d <= 3:
                cli.send_text("talk " + target["name"])
            else:
                walk(dir_toward(tx - st.x, ty - st.y))
        elif target["type"] == OBJ_MONSTER:
            if d == 1:
                cli.send_text("attack %d" % dir_toward(tx - st.x, ty - st.y))
            else:
                walk(dir_toward(tx - st.x, ty - st.y))
        else:
            if d == 0:
                cli.send_text("pickup")
            else:
                walk(dir_toward(tx - st.x, ty - st.y))

    canvas.bind("<Button-1>", on_canvas_click)

    def open_quests():
        win = tk.Toplevel(root)
        win.title("任务")
        win.configure(bg="#1b1b1f")
        win.geometry("460x460")
        body = tk.Frame(win, bg="#1b1b1f")
        body.pack(fill="both", expand=True, padx=8, pady=8)

        def fill():
            if not win.winfo_exists():
                return
            for w in body.winfo_children():
                w.destroy()
            if not st.quests:
                tk.Label(body, text="没有任务数据", fg="#888888", bg="#1b1b1f").pack()
            for q in st.quests:
                state = {0: "可接", 1: "进行中", 2: "已完成"}.get(q["state"], "?")
                color = "#e8c37a" if q["state"] == 1 else ("#4fb06d" if q["state"] == 2 else "#cccccc")
                text = "[%s] %s  %s %d/%d  奖励 %d 经验 / %d 金币" % (
                    state, q["name"], q["target"], q["progress"], q["need"], q["exp"], q["gold"])
                tk.Label(body, text=text, fg=color, bg="#1b1b1f", anchor="w",
                         font=("PingFang SC", 11)).pack(fill="x", pady=1)

        cli.send_text("questlog")
        win.after(300, fill)

    def open_titles():
        win = tk.Toplevel(root)
        win.title("称号收集")
        win.configure(bg="#1b1b1f")
        win.geometry("460x480")
        body = tk.Frame(win, bg="#1b1b1f")
        body.pack(fill="both", expand=True, padx=8, pady=8)

        def send(cmd):
            cli.send_text(cmd)
            win.after(250, fill)

        def fill():
            if not win.winfo_exists():
                return
            for w in body.winfo_children():
                w.destroy()
            tk.Label(body, text="可用成就点：%d" % st.available_points,
                     fg="#e8c37a", bg="#1b1b1f", font=("PingFang SC", 11, "bold")).pack(anchor="w", pady=4)
            for t in st.titles:
                row = tk.Frame(body, bg="#1b1b1f")
                row.pack(fill="x", pady=2)
                if t["source"] == 0:
                    src = "成就"
                elif t["cost"] < 0:
                    src = "图鉴"
                elif t["cost"] == 0:
                    src = "首通"
                else:
                    src = "兑换"
                text = "[%s] %s%s" % (src, t["name"],
                                      ("  %d 点" % t["cost"]) if t["source"] == 1 and t["cost"] > 0 else "")
                color = "#4fb06d" if t["active"] else ("#dddddd" if t["owned"] else "#888888")
                tk.Label(row, text=text, fg=color, bg="#1b1b1f", width=24,
                         anchor="w").pack(side="left")
                if t["active"]:
                    tk.Label(row, text="佩戴中", fg="#e8c37a", bg="#1b1b1f").pack(side="left")
                elif t["owned"]:
                    tk.Button(row, text="佩戴", command=lambda i=t: send("usetitle %d" % i["id"]),
                              bg="#3fa7d6", fg="#101014", relief="flat").pack(side="left")
                elif t["source"] == 1:
                    tk.Button(row, text="兑换", command=lambda i=t: send("buytitle %d" % i["id"]),
                              bg="#e8c37a", fg="#101014", relief="flat").pack(side="left")

        cli.send_text("titlelist")
        win.after(300, fill)

    def open_codex():
        win = tk.Toplevel(root)
        win.title("套装图鉴")
        win.configure(bg="#1b1b1f")
        win.geometry("460x500")
        body = tk.Frame(win, bg="#1b1b1f")
        body.pack(fill="both", expand=True, padx=8, pady=8)

        def fill():
            if not win.winfo_exists():
                return
            for w in body.winfo_children():
                w.destroy()
            c = st.codex
            tk.Label(body, text="物品 %d 件    称号 %d 个    成就 %d 条" % (
                c["items"], c["titles"], c["ach"]),
                fg="#e8c37a", bg="#1b1b1f", font=("PingFang SC", 11, "bold")).pack(anchor="w", pady=4)
            for s_ in c["sets"]:
                row = tk.Frame(body, bg="#1b1b1f")
                row.pack(fill="x", pady=2)
                color = "#e8c37a" if s_["complete"] else "#cccccc"
                tk.Label(row, text="%s  %d/%d" % (s_["name"], s_["found"], s_["total"]),
                         fg=color, bg="#1b1b1f", width=20, anchor="w").pack(side="left")
                bar = tk.Canvas(row, width=120, height=10, bg="#26262c", highlightthickness=0)
                bar.pack(side="left")
                ratio = 0 if s_["total"] == 0 else s_["found"] / float(s_["total"])
                bar.create_rectangle(0, 0, int(120 * ratio), 10, fill="#4fb06d", outline="")
                if s_["complete"]:
                    tk.Label(row, text="已集齐", fg="#e8c37a", bg="#1b1b1f").pack(side="left", padx=4)

        cli.send_text("codex")
        win.after(300, fill)

    def cast_index(idx):
        if 0 <= idx < len(skill_buttons):
            cast_skill(skill_buttons[idx][1])

    for i in range(8):
        root.bind(str(i + 1), lambda e, idx=i: cast_index(idx))

    def open_panel(kind):
        titles = {"bag": "背包", "equip": "装备", "shop": "商店"}
        win = tk.Toplevel(root)
        win.title(titles[kind])
        win.configure(bg="#1b1b1f")
        win.geometry("360x420")
        body = tk.Frame(win, bg="#1b1b1f")
        body.pack(fill="both", expand=True, padx=8, pady=8)

        def send(cmd):
            cli.send_text(cmd)
            win.after(250, fill)

        def fill():
            if not win.winfo_exists():
                return
            for w in body.winfo_children():
                w.destroy()

            if kind == "bag":
                if not st.bag:
                    tk.Label(body, text="背包是空的", fg="#888888", bg="#1b1b1f").pack()
                for it in st.bag:
                    row = tk.Frame(body, bg="#1b1b1f")
                    row.pack(fill="x", pady=2)
                    elem = {1: "火", 2: "冰", 3: "雷", 4: "毒"}.get(it.get("elem"), "")
                    label = "%s%s%s x%d  耐久 %d/%d" % (
                        it["name"], (" +%d" % it["up"]) if it["up"] else "",
                        (" [%s+%d]" % (elem, it.get("epow", 0))) if elem else "",
                        it["count"], it["dura"], it["maxdura"])
                    tk.Label(row, text=label, fg="#dddddd", bg="#1b1b1f",
                             image=item_icon(it["name"]), compound="left",
                             anchor="w", width=22).pack(side="left")
                    if it["usetype"]:
                        tk.Button(row, text="使用", command=lambda i=it: send("use %d" % i["id"]),
                                  bg="#4fb06d", fg="#101014", relief="flat").pack(side="left", padx=1)
                    tk.Button(row, text="装备", command=lambda i=it: send("equipauto %d" % i["id"]),
                              bg="#3fa7d6", fg="#101014", relief="flat").pack(side="left", padx=1)
                    tk.Button(row, text="出售", command=lambda i=it: send("sellitem %d" % i["id"]),
                              bg="#e8c37a", fg="#101014", relief="flat").pack(side="left", padx=1)
            elif kind == "equip":
                if not st.equip:
                    tk.Label(body, text="没有装备", fg="#888888", bg="#1b1b1f").pack()
                for it in st.equip:
                    row = tk.Frame(body, bg="#1b1b1f")
                    row.pack(fill="x", pady=2)
                    elem = {1: "火", 2: "冰", 3: "雷", 4: "毒"}.get(it.get("elem"), "")
                    label = "[%d] %s%s%s  耐久 %d/%d" % (
                        it["pos"], it["name"], (" +%d" % it["up"]) if it["up"] else "",
                        (" [%s+%d]" % (elem, it.get("epow", 0))) if elem else "",
                        it["dura"], it["maxdura"])
                    tk.Label(row, text=label, fg="#dddddd", bg="#1b1b1f",
                             image=item_icon(it["name"]), compound="left",
                             anchor="w", width=24).pack(side="left")
                    tk.Button(row, text="卸下", command=lambda i=it: send("unequip %d %d" % (i["pos"], i["id"])),
                              bg="#d65a3f", fg="#ffffff", relief="flat").pack(side="left")
            else:
                if not st.shop:
                    tk.Label(body, text="货架为空", fg="#888888", bg="#1b1b1f").pack()
                for it in st.shop:
                    row = tk.Frame(body, bg="#1b1b1f")
                    row.pack(fill="x", pady=2)
                    tk.Label(row, text="%s  %d 金币" % (it["name"], it["price"]),
                             fg="#dddddd", bg="#1b1b1f", image=item_icon(it["name"]),
                             compound="left", anchor="w", width=22).pack(side="left")
                    tk.Button(row, text="购买", command=lambda i=it: send("buy %s 1" % i["name"]),
                              bg="#4fb06d", fg="#101014", relief="flat").pack(side="left")

        def request():
            cli.send_text({"bag": "bag", "equip": "equipment", "shop": "shop"}[kind])
            win.after(300, fill)

        request()

    root.bind("<b>", lambda e: open_panel("bag"))
    root.bind("<B>", lambda e: open_panel("bag"))
    root.bind("<c>", lambda e: open_panel("equip"))
    root.bind("<C>", lambda e: open_panel("equip"))
    root.bind("<v>", lambda e: open_panel("shop"))
    root.bind("<V>", lambda e: open_panel("shop"))
    root.bind("<q>", lambda e: open_quests())
    root.bind("<Q>", lambda e: open_quests())
    root.bind("<l>", lambda e: open_titles())
    root.bind("<L>", lambda e: open_titles())
    root.bind("<k>", lambda e: open_codex())
    root.bind("<K>", lambda e: open_codex())

    last_terrain = {"x": -9999, "y": -9999, "t": 0.0}

    def tick():
        if not hasattr(tick, "n"):
            tick.n = 0
        tick.n += 1
        if tick.n == 1:
            print("tick started", flush=True)
        if tick.n == 10:
            print("canvas", canvas.winfo_width(), canvas.winfo_height(),
                  "mapped", canvas.winfo_ismapped(),
                  "items", len(canvas.find_all()),
                  "terrain", st.terrain is not None,
                  "ents", len(st.entities), flush=True)
        cli.poll()

        now = time.time()
        if st.terrain is None or (st.x, st.y) != (last_terrain["x"], last_terrain["y"]):
            if now - last_terrain["t"] > 0.25:
                last_terrain.update(x=st.x, y=st.y, t=now)
                cli.send_text("mapdata")
        while shown["chat"] < len(st.chat):
            log_append(st.chat[shown["chat"]])
            shown["chat"] += 1
        st.anim = int(time.time() * 2) % 2
        draw_map()
        draw_bars()
        refresh_skillbar()
        root.after(80, tick)

    def do_connect():
        cls_map = {"战士": "0", "法师": "1", "道士": "2"}
        try:
            cls_id = cls_map.get(class_var.get(), "0")
            st.player_class = int(cls_id)
            cli.connect(name_var.get().strip() or "玩家", cls_id)
        except OSError as exc:
            status_var.set("连接失败：%s" % exc)
            return
        cli.send_text("skills")
        cli.send_text("mapdata")
        if login.winfo_exists():
            login.destroy()
        game.pack(fill="both", expand=True)
        print("do_connect: game packed", flush=True)
        root.after(80, tick)
        entry.focus_set()
        log_append("已连接，等待服务器数据……")
        log_append("方向键/WASD 走路，空格攻击，E 拾取，T 对话，B 背包，C 装备，V 商店")
        log_append("1-8 技能，点击交互，Q 任务，L 称号，K 图鉴")
        log_append("聊天框输入 /cast 火球术 或 /quests 等命令")

    tk.Button(login, text="进入游戏", command=do_connect, bg="#e8c37a", fg="#101014",
              relief="flat", font=("PingFang SC", 13, "bold"), padx=24, pady=6).pack(pady=16)

    if args.auto:
        #	自动模式：直接销毁登录页，只保留游戏界面
        login.pack_forget()
        login.destroy()
        root.after(300, do_connect)

    if args.gui_selftest:
        #	自检：隐藏窗口，自动登录、打开三个面板并渲染
        root.withdraw()
        root.after(100, do_connect)
        root.after(700, lambda: open_panel("bag"))
        root.after(1000, lambda: open_panel("equip"))
        root.after(1300, lambda: open_panel("shop"))
        root.after(1600, open_quests)
        root.after(3000, root.destroy)

    root.mainloop()
    return 0


def main():
    ap = argparse.ArgumentParser(description="GameServer 图形客户端")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7100)
    ap.add_argument("--headless", action="store_true", help="不启动 GUI，仅自动验收")
    ap.add_argument("--name", default="测试玩家")
    ap.add_argument("--class", dest="cls", default="0", help="0 战 / 1 法 / 2 道")
    ap.add_argument("--seconds", type=float, default=1.5)
    ap.add_argument("--commands", default="", help="headless 下发送的命令，用 | 分隔")
    ap.add_argument("--gui-selftest", action="store_true", help="隐藏窗口自检 GUI 渲染")
    ap.add_argument("--auto", action="store_true", help="启动后自动登录进入游戏")
    args = ap.parse_args()
    if args.headless:
        return run_headless(args)
    return run_gui(args)


if __name__ == "__main__":
    raise SystemExit(main())
