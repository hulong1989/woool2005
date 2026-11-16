#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""传奇世界 · 桌面客户端（Pygame）

不依赖浏览器，直接连接 GameServer:7100。
账号登录 → 角色选择/创建 → 等距世界 + HUD + 面板。
"""
import hashlib
import math
import os
import struct
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLIENT_DIR = os.path.join(ROOT, "client")
sys.path.insert(0, os.path.join(ROOT, "vendor"))
sys.path.insert(0, CLIENT_DIR)

import pygame  # noqa: E402
import game_client as gc  # noqa: E402

W, H = 1100, 700
TW, TH = 64, 32
FONT_PATH = "/System/Library/Fonts/STHeiti Medium.ttc"
ASSETS = os.path.join(CLIENT_DIR, "assets")
PORT = int(os.environ.get("WOOOL_PORT", "7101"))
PLAYER_STYLE = os.environ.get("WOOOL_PLAYER", "dcss")
GOLD = (201, 162, 74)
PALE = (240, 228, 200)
DARK = (20, 15, 9)
RED = (190, 70, 55)
BLUE = (60, 90, 170)
GREEN = (80, 150, 95)

pygame.init()
pygame.display.set_caption("传奇世界 · 桌面客户端")
screen = pygame.display.set_mode((W, H))
clock = pygame.time.Clock()

_fonts = {}
def font(size):
    if size not in _fonts:
        _fonts[size] = pygame.font.Font(FONT_PATH, size)
    return _fonts[size]

def text(s, size, color):
    return font(size).render(str(s), True, color)

SKIN = {}

def tint(surf, color):
    if surf is None:
        return None
    try:
        out = pygame.transform.grayscale(surf)
    except Exception:
        out = surf.copy()
    out.fill((*color, 255), special_flags=pygame.BLEND_RGBA_MULT)
    return out

def hover(rect):
    return rect.collidepoint(pygame.mouse.get_pos())

def panel996(rect, title=None):
    pygame.draw.rect(screen, (5, 6, 9), rect)
    if SKIN.get("panel"):
        screen.blit(pygame.transform.smoothscale(SKIN["panel"], rect.size), rect.topleft)
    else:
        pygame.draw.rect(screen, (25, 30, 39), rect.inflate(-6, -6))
        pygame.draw.rect(screen, (120, 128, 140), rect, 2)
        pygame.draw.rect(screen, (145, 112, 52), rect.inflate(-10, -10), 1)
    for x, y in ((rect.x + 4, rect.y + 4), (rect.right - 12, rect.y + 4),
                 (rect.x + 4, rect.bottom - 12), (rect.right - 12, rect.bottom - 12)):
        pygame.draw.rect(screen, (190, 160, 90), (x, y, 8, 8))
    if title:
        bar = pygame.Rect(rect.x + 12, rect.y + 8, rect.w - 24, 28)
        if SKIN.get("ribbon"):
            screen.blit(pygame.transform.smoothscale(SKIN["ribbon"], bar.size), bar.topleft)
        else:
            pygame.draw.rect(screen, (70, 18, 16), bar)
        pygame.draw.rect(screen, (170, 120, 55), bar, 1)
        t = text(title, 15, (255, 236, 190))
        screen.blit(t, (bar.centerx - t.get_width() // 2, bar.y + 5))

def button996(rect, label, size=12, active=False):
    h = hover(rect)
    base = (92, 100, 115) if h else ((112, 84, 36) if active else (38, 44, 56))
    pygame.draw.rect(screen, base, rect)
    pygame.draw.rect(screen, (160, 168, 180), rect, 1)
    pygame.draw.line(screen, (210, 215, 225), (rect.x + 1, rect.y + 1), (rect.right - 2, rect.y + 1))
    if SKIN.get("gem"):
        g = pygame.transform.smoothscale(SKIN["gem"], (16, 16))
        screen.blit(g, (rect.x + 3, rect.centery - 8))
    t = text(label, size, (248, 242, 226))
    screen.blit(t, (rect.centerx - t.get_width() // 2 + 6, rect.centery - t.get_height() // 2))

def load_img(path, scale=None):
    try:
        im = pygame.image.load(path).convert_alpha()
        if scale:
            im = pygame.transform.smoothscale(im, scale)
        return im
    except Exception:
        return None

class Desktop:
    def __init__(self):
        self.screen_name = "login"
        self.active = None
        self.buttons = []
        self.account = "网页演示"
        self.password = ""
        self.cls = 0
        self.msg = ""
        self.client = None
        self.token = ""
        self.state = None
        self.last_poll = time.time()
        self.last_social = 0
        self.panel = None
        self.chat_input = ""
        self.chat_active = False
        self.target = None
        self.move_goal = None
        self.last_step = 0
        self.last_attack = 0
        self.own_attack_until = 0
        self.hover = None
        self.cam = [0, 0]
        self.cam_ready = False
        self.new_char = ""
        self.new_class = 0
        self.counter = 0
        self.load_assets()

    def load_assets(self):
        base = os.path.join(ASSETS, "thirdparty")
        stone = os.path.join(base, "stone_soup")
        self.floor = {
            "grass": load_img(os.path.join(stone, "grass.gif")),
            "grass2": load_img(os.path.join(stone, "grass2.gif")),
            "grass3": load_img(os.path.join(stone, "grass3.gif")),
            "crypt": load_img(os.path.join(stone, "crypt_lit.gif")),
            "wall": load_img(os.path.join(stone, "wall.gif")),
            "wall2": load_img(os.path.join(stone, "wall2.gif")),
        }
        self.sprites = {}
        for fn in os.listdir(os.path.join(base, "dcss")):
            if fn.endswith(".gif"):
                self.sprites[fn] = load_img(os.path.join(base, "dcss", fn), (64, 64))
        self.frames = {}
        fpath = os.path.join(base, "dcss_frames", "frames.json")
        try:
            import json
            man = json.load(open(fpath, encoding="utf-8"))
            for gif, files in man.items():
                self.frames[gif] = [load_img(os.path.join(base, "dcss_frames", f), (64, 64))
                                    for f in files]
        except Exception:
            pass
        self.clint_raw = {}
        self.clint_cache = {}
        cdir = os.path.join(base, "clint_iso")
        for fn in os.listdir(cdir):
            if not fn.endswith(".png"):
                continue
            im = load_img(os.path.join(cdir, fn))
            if not im or im.get_width() < 2048:
                continue
            rows = []
            for r in range(8):
                row = []
                for c in range(8):
                    fr = im.subsurface(pygame.Rect(c * 256, r * 256, 256, 256)).copy()
                    row.append(pygame.transform.smoothscale(fr, (96, 96)))
                rows.append(row)
            self.clint_raw[fn] = rows
        self.ui = {
            "panel": load_img(os.path.join(base, "fantasy_gui", "panel_small.png")),
            "wide": load_img(os.path.join(base, "fantasy_gui", "panel_wide.png")),
            "ribbon": load_img(os.path.join(base, "fantasy_gui", "ribbon.png")),
            "gem": load_img(os.path.join(base, "fantasy_gui", "gem_gold.png")),
            "gem_p": load_img(os.path.join(base, "fantasy_gui", "gem_purple.png")),
            "gem_r": load_img(os.path.join(base, "fantasy_gui", "gem_red.png")),
            "orb": load_img(os.path.join(base, "fantasy_gui", "orb.png")),
            "parchment": load_img(os.path.join(base, "fantasy_gui", "parchment.png")),
        }
        SKIN["panel"] = tint(self.ui.get("panel"), (105, 120, 145))
        SKIN["wide"] = tint(self.ui.get("wide"), (105, 120, 145))
        SKIN["ribbon"] = self.ui.get("ribbon")
        SKIN["gem"] = self.ui.get("gem")
        SKIN["gem_p"] = self.ui.get("gem_p")
        SKIN["orb"] = self.ui.get("orb")
        self.minimap = pygame.Surface((190, 120))
        self.inputs = {}
        self.field = None
        self.char_selected = None
        self.create_mode = False
        self.confirm_delete = None
        self.rename_old = None
        self.tooltip = None
        self.anim_last = {}
        self.dialog_dismissed = ""
        self.cd = {}
        self.own_attack_until = 0
        self.own_last = None
        self.last_level = 0
        self.last_effects = 0
        self.sounds = {}
        try:
            pygame.mixer.init()
            for nm in ("hit", "spell", "level"):
                path = os.path.join(ASSETS, nm + ".wav")
                if os.path.exists(path):
                    self.sounds[nm] = pygame.mixer.Sound(path)
        except Exception:
            pass
        self.item_icons = {}
        for key, fn in {
            "sword": "long_sword1.gif", "axe": "axe.gif", "staff": "quarterstaff.gif",
            "mace": "mace1.gif", "armor": "armor.gif", "robe": "robe.gif",
            "plate": "plate.gif", "helmet": "helmet.gif", "ring": "ring.gif",
            "amulet": "amulet.gif", "potion_hp": "potion_hp.gif", "potion_mp": "potion_mp.gif",
            "book": "book.gif", "food": "food.gif", "gold": "gold.gif", "misc": "misc.gif",
        }.items():
            im = load_img(os.path.join(base, "dcss", fn), (28, 28))
            if im:
                self.item_icons[key] = im

    def play(self, name):
        snd = self.sounds.get(name)
        if snd:
            try:
                snd.play()
            except Exception:
                pass

    def icon_for(self, name):
        if "剑" in name or "刀" in name: key = "sword"
        elif "斧" in name: key = "axe"
        elif "杖" in name or "扇" in name: key = "staff"
        elif "锤" in name: key = "mace"
        elif "袍" in name: key = "robe"
        elif "甲" in name or "衣" in name or "服" in name: key = "armor"
        elif "盔" in name: key = "helmet"
        elif "项链" in name or "铃" in name: key = "amulet"
        elif "戒指" in name or "手镯" in name: key = "ring"
        elif "药" in name: key = "potion_hp" if "魔法" not in name else "potion_mp"
        elif "书" in name: key = "book"
        elif "金币" in name: key = "gold"
        else: key = "misc"
        return self.item_icons.get(key)

    def field_box(self, rect, key, placeholder=""):
        pygame.draw.rect(screen, (15, 12, 8), rect)
        pygame.draw.rect(screen, GOLD if self.field == key else (90, 70, 40), rect, 1)
        val = self.inputs.get(key, "")
        screen.blit(text(val or placeholder, 12, PALE), (rect.x + 6, rect.y + 4))
        self.buttons.append((rect, lambda: setattr(self, "field", key)))

    def cmd_field(self, key):
        return self.inputs.get(key, "").strip()

    def dialog_key(self, d):
        return d["npc"] + "|" + d["text"] + "|" + str([o["index"] for o in d.get("options", [])])

    def cast_skill(self, name):
        sk = next((s for s in (self.state.skills or []) if s["name"] == name), None)
        if not sk or not sk["learned"]:
            return
        if time.time() < self.cd.get(name, 0):
            return
        self.cmd("cast " + name)
        self.cd[name] = time.time() + sk["cd"] / 1000.0

    # ---------------------------------------------------------------- network
    def cmd(self, text):
        if self.client is None:
            return
        if text.startswith("attack"):
            self.own_attack_until = time.time() + 0.3
            self.play("attack")
        elif text.startswith("cast"):
            self.own_attack_until = time.time() + 0.35
            self.play("spell")
        if self.token:
            self.client.send_text(self.secure(text))
        else:
            self.client.send_text(text)

    def secure(self, text):
        self.counter = getattr(self, "counter", 0) + 1
        seed = self.token.encode() + struct.pack("<Q", self.counter)
        ks = hashlib.sha256(seed).hexdigest()
        data = text.encode()
        cipher = bytes(b ^ ord(ks[i % len(ks)]) for i, b in enumerate(data))
        return "enc %x %s" % (self.counter, cipher.hex())

    def wait_token(self, timeout=3.0):
        end = time.time() + timeout
        while time.time() < end:
            self.client.poll()
            if self.client.state.auth_token:
                self.token = self.client.state.auth_token
                return True
            if self.client.state.chat and any(w in self.client.state.chat[-1] for w in ("失败", "错误", "不存在")):
                self.msg = self.client.state.chat[-1]
                return False
            time.sleep(0.04)
        return False

    def login(self, register=False):
        name = self.account.strip()
        if not name or not self.password:
            self.msg = "账号和密码不能为空"
            return
        self.msg = "连接中…"
        self.state = gc.GameState()
        try:
            self.client = gc.GameClient("127.0.0.1", PORT, self.state)
            if register:
                self.client.connect(name, str(self.cls), command="register %s %s %d" % (name, self.password, self.cls))
                time.sleep(0.2)
                self.client.send_text("login %s %s %d" % (name, self.password, self.cls))
            else:
                self.client.connect(name, str(self.cls), command="login %s %s %d" % (name, self.password, self.cls))
        except OSError as exc:
            self.msg = "连接失败：%s" % exc
            return
        if not self.wait_token():
            self.msg = self.msg or "登录失败"
            return
        self.cmd("charlist")
        self.screen_name = "chars"
        self.msg = ""

    def select_char(self, name):
        ch = next((c for c in self.state.characters if c["name"] == name), None)
        if ch:
            self.state.player_class = ch["cls"]
        self.cmd("selectchar " + name)
        end = time.time() + 3
        while time.time() < end:
            self.client.poll()
            if self.state.name == name:
                self.screen_name = "game"
                self.msg = ""
                for cmd in ("skills", "mapdata", "bag", "equipment", "shop", "questlog",
                            "titlelist", "codex", "partylist", "guildinfo", "dungeonlist"):
                    self.cmd(cmd)
                return
            time.sleep(0.03)
        self.msg = "进入角色失败"

    def create_char(self):
        name = self.new_char.strip()
        if len(name) < 2:
            self.msg = "角色名至少 2 个字符"
            return
        self.cmd("createchar %s %d" % (name, self.new_class))
        time.sleep(0.4)
        self.cmd("charlist")
        self.new_char = ""
        self.create_mode = False

    # ---------------------------------------------------------------- helpers
    def terrain_at(self, wx, wy):
        t = self.state.terrain if self.state else None
        if not t:
            return 2
        x, y = wx - t["ox"], wy - t["oy"]
        if x < 0 or y < 0 or x >= t["w"] or y >= t["h"]:
            return 2
        i = y * t["w"] + x
        return t["blocks"][i] if i < len(t["blocks"]) else 2

    def clint_frames(self, sheet, color):
        key = (sheet, color)
        if key in self.clint_cache:
            return self.clint_cache[key]
        raw = self.clint_raw.get(sheet)
        if not raw:
            return None
        rows = []
        for row in raw:
            frames = []
            for fr in row:
                mask = pygame.mask.from_surface(fr)
                out = mask.to_surface(setcolor=(*color, 255), unsetcolor=(0, 0, 0, 0))
                shade = fr.copy()
                shade.set_alpha(70)
                out.blit(shade, (0, 0))
                frames.append(out)
            rows.append(frames)
        self.clint_cache[key] = rows
        return rows

    def sprite_for(self, ent):
        d = ent.get("dir", getattr(self.state, "dir", 4) if self.state else 4)
        d = max(0, min(7, int(d)))
        # 玩家
        if ent.get("type") == 5:
            cls = getattr(self.state, "player_class", 0) if self.state else 0
            if PLAYER_STYLE == "dcss":
                fn = "warrior.gif" if cls == 0 else ("mage.gif" if cls == 1 else "taoist.gif")
                return self.frames.get(fn) or [self.sprites.get(fn)] * 3
            sheet, col = (("male_heavy.png", (185, 205, 235)) if cls == 0 else
                          ("male_light.png", (190, 170, 245)) if cls == 1 else
                          ("male_unarmored.png", (165, 220, 180)))
            rows = self.clint_frames(sheet, col)
            return rows[d] if rows else [None] * 8
        name = ent.get("name", "")
        # NPC
        if ent.get("type") == 4:
            return self.frames.get("npc.gif") or [self.sprites.get("npc.gif")] * 3
        # 怪物继续用 CC0 DCSS 彩色精灵
        man = {
            "鸡": "chicken.gif", "鹿": "deer.gif", "稻草人": "dummy.gif",
            "多钩猫": "hound.gif", "钉耙猫": "warg.gif", "半兽人": "orc.gif",
            "半兽战士": "orc.gif", "沃玛教主": "orc_warlord.gif",
            "沃玛护卫": "orc_warrior.gif", "沃玛战士": "orc_knight.gif",
            "骷髅": "skeleton.gif", "骷髅王": "champion.gif", "僵尸": "ghoul.gif",
            "尸王": "mummy.gif", "蛇": "adder.gif", "蝎子": "scorpion.gif",
            "蛇魔": "anaconda.gif", "祖玛卫士": "gargoyle.gif",
            "祖玛雕像": "statue.gif", "祖玛教主": "golem.gif",
            "逆魔卫士": "devil.gif", "逆魔雕像": "obsidian.gif",
            "逆魔教主": "shadow_demon.gif", "禁地骷髅": "skeleton.gif",
            "禁地护卫": "death_knight.gif", "禁地魔王": "dread_lich.gif",
            "骷髅宝宝": "skeleton.gif", "神兽": "warg.gif",
        }
        n = name
        fn = man.get(n)
        if fn is None:
            for k, v in man.items():
                if n.startswith(k):
                    fn = v; break
        fn = fn or "orc.gif"
        return self.frames.get(fn) or [self.sprites.get(fn)] * 3

    def iso(self, wx, wy):
        dx, dy = wx - self.cam[0], wy - self.cam[1]
        return int(W / 2 + (dx - dy) * TW / 2), int(H / 2 + 40 + (dx + dy) * TH / 2)

    # -------------------------------------------------------------- rendering
    def draw_bg(self):
        screen.fill((8, 8, 10))

    def draw_login(self):
        self.draw_bg()
        title = text("传 奇 世 界", 40, PALE)
        screen.blit(title, (W // 2 - title.get_width() // 2, 120))
        sub = text("桌面客户端 · 账号登录", 14, GOLD)
        screen.blit(sub, (W // 2 - sub.get_width() // 2, 175))
        panel = pygame.Rect(W // 2 - 210, 230, 420, 290)
        panel996(panel, "账号登录")
        # 区服
        screen.blit(text("区服", 13, GOLD), (panel.x + 30, panel.y + 55))
        srv = pygame.Rect(panel.x + 90, panel.y + 50, 130, 28)
        button996(srv, "经典一区", 12, True)
        self.buttons.append((srv, lambda: None))
        labels = [("账号", self.account, "account"), ("密码", "*" * len(self.password), "password")]
        y = panel.y + 105
        self.buttons = []
        for lab, val, key in labels:
            screen.blit(text(lab, 13, GOLD), (panel.x + 30, y + 5))
            r = pygame.Rect(panel.x + 90, y, 290, 30)
            pygame.draw.rect(screen, (12, 14, 20), r)
            pygame.draw.rect(screen, (180, 150, 80) if self.active == key else (100, 105, 118), r, 1)
            screen.blit(text(val or ("点击输入" if key == "account" else ""), 14, PALE), (r.x + 8, r.y + 5))
            self.buttons.append((r, lambda k=key: setattr(self, "active", k)))
            y += 50
        screen.blit(text("登录后可选择、建立或删除角色", 11, (150, 160, 175)), (panel.x + 30, y + 5))
        b1 = pygame.Rect(panel.x + 30, panel.bottom - 62, 160, 36)
        b2 = pygame.Rect(panel.right - 190, panel.bottom - 62, 160, 36)
        for r, lab, cb in ((b1, "登 录", lambda: self.login(False)), (b2, "注 册", lambda: self.login(True))):
            button996(r, lab, 15, True)
            self.buttons.append((r, cb))
        if self.msg:
            m = text(self.msg, 12, (230, 140, 100))
            screen.blit(m, (panel.x + 30, panel.bottom - 20))

    def draw_chars(self):
        self.draw_bg()
        title = text("选 择 角 色", 32, (240, 226, 190))
        screen.blit(title, (W // 2 - title.get_width() // 2, 45))
        sub = text("传奇世界 · 角色选择", 12, GOLD)
        screen.blit(sub, (W // 2 - sub.get_width() // 2, 86))

        chars = self.state.characters if self.state else []
        if not self.char_selected or not any(c["name"] == self.char_selected for c in chars):
            self.char_selected = chars[0]["name"] if chars else None
        self.buttons = []
        names = ("战士", "法师", "道士")

        left = pygame.Rect(34, 112, 300, 460)
        panel996(left, "角色列表")
        for i in range(3):
            r = pygame.Rect(left.x + 18, left.y + 54 + i * 98, left.w - 36, 82)
            ch = chars[i] if i < len(chars) else None
            sel = ch is not None and ch["name"] == self.char_selected
            pygame.draw.rect(screen, (24, 30, 40) if not sel else (70, 58, 30), r)
            pygame.draw.rect(screen, (180, 150, 80) if sel else (90, 100, 115), r, 2 if sel else 1)
            if ch:
                screen.blit(text(ch["name"][:9], 15, PALE), (r.x + 12, r.y + 10))
                screen.blit(text("%s  Lv.%d" % (names[ch["cls"]], ch["level"]), 12, GOLD),
                            (r.x + 12, r.y + 38))
                screen.blit(text("点击选择", 10, (150, 165, 180)), (r.x + 12, r.y + 60))
                self.buttons.append((r, lambda n=ch["name"]: self.choose_char(n)))
            else:
                screen.blit(text("空角色槽", 13, (105, 115, 130)), (r.x + 12, r.y + 30))

        center = pygame.Rect(350, 112, 350, 460)
        panel996(center)
        if self.char_selected:
            ch = next(c for c in chars if c["name"] == self.char_selected)
            fn = "warrior.gif" if ch["cls"] == 0 else ("mage.gif" if ch["cls"] == 1 else "taoist.gif")
            frames = self.frames.get(fn) or [self.sprites.get(fn)]
            img = frames[0] if frames else None
            if img:
                big = pygame.transform.smoothscale(img, (170, 170))
                screen.blit(big, (center.centerx - 85, center.y + 80))
            screen.blit(text(ch["name"], 20, PALE),
                        (center.centerx - text(ch["name"], 20, PALE).get_width() // 2, center.y + 280))
            line = "%s · Lv.%d" % (names[ch["cls"]], ch["level"])
            screen.blit(text(line, 14, GOLD),
                        (center.centerx - text(line, 14, GOLD).get_width() // 2, center.y + 315))
        else:
            screen.blit(text("尚未建立角色", 14, (150, 160, 175)),
                        (center.centerx - 60, center.y + 210))

        right = pygame.Rect(716, 112, 350, 460)
        panel996(right, "角色信息")
        if self.char_selected:
            ch = next(c for c in chars if c["name"] == self.char_selected)
            info = [("名字", ch["name"]), ("职业", names[ch["cls"]]), ("等级", "Lv.%d" % ch["level"]),
                    ("状态", "可进入游戏")]
            yy = right.y + 62
            for k, v in info:
                screen.blit(text(k, 12, (150, 160, 175)), (right.x + 28, yy))
                screen.blit(text(v, 14, PALE), (right.x + 100, yy - 2))
                yy += 42
        else:
            screen.blit(text("请先建立角色", 13, (150, 160, 175)), (right.x + 28, right.y + 70))

        # 底部操作
        by = 590
        enter = pygame.Rect(300, by, 130, 34)
        create = pygame.Rect(445, by, 130, 34)
        delete = pygame.Rect(590, by, 130, 34)
        back = pygame.Rect(735, by, 130, 34)
        button996(enter, "进入游戏", 13, self.char_selected is not None)
        button996(create, "建立角色", 13, False)
        button996(delete, "确认删除" if self.confirm_delete == self.char_selected else "删除角色",
                  13, False)
        button996(back, "返回登录", 13, False)
        self.buttons.append((enter, lambda: self.select_char(self.char_selected) if self.char_selected else None))
        self.buttons.append((create, lambda: setattr(self, "create_mode", True)))
        self.buttons.append((delete, lambda: self.delete_char(self.char_selected) if self.char_selected else None))
        self.buttons.append((back, self.back_login))
        if self.msg:
            m = text(self.msg, 12, (235, 150, 110))
            screen.blit(m, (W // 2 - m.get_width() // 2, 640))

        if self.create_mode:
            self.draw_create_char()

    def draw_create_char(self):
        overlay = pygame.Surface((W, H), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 170))
        screen.blit(overlay, (0, 0))
        box = pygame.Rect(W // 2 - 230, 170, 460, 300)
        panel996(box, "建立角色")
        screen.blit(text("角色名", 13, GOLD), (box.x + 34, box.y + 64))
        self.field_box(pygame.Rect(box.x + 100, box.y + 58, 300, 32), "newchar", "输入角色名")
        screen.blit(text("职业", 13, GOLD), (box.x + 34, box.y + 120))
        for i, nm in enumerate(("战士", "法师", "道士")):
            r = pygame.Rect(box.x + 100 + i * 96, box.y + 114, 90, 32)
            button996(r, nm, 12, self.new_class == i)
            self.buttons.append((r, lambda i=i: setattr(self, "new_class", i)))
        ok = pygame.Rect(box.x + 100, box.y + 190, 120, 34)
        cancel = pygame.Rect(box.x + 240, box.y + 190, 120, 34)
        button996(ok, "创建角色", 13, True)
        button996(cancel, "取消", 13, False)
        self.buttons.append((ok, self.create_char))
        self.buttons.append((cancel, lambda: setattr(self, "create_mode", False)))

    def choose_char(self, name):
        self.char_selected = name
        self.confirm_delete = None
        self.msg = ""

    def delete_char(self, name):
        if self.confirm_delete != name:
            self.confirm_delete = name
            self.msg = "再次点击「删除角色」确认删除：" + name
            return
        self.confirm_delete = None
        self.cmd("deletechar " + name)
        self.msg = "已发送删除请求"
        self.char_selected = None
        time.sleep(0.3)
        self.cmd("charlist")

    def begin_rename(self, name):
        self.rename_old = name
        self.inputs["rename_name"] = name
        self.field = "rename_name"
        self.msg = "编辑新角色名后点改名"

    def do_rename(self):
        new = self.cmd_field("rename_name")
        if not self.rename_old or not new or new == self.rename_old:
            self.msg = "请先选中角色并输入新名字"
            return
        self.cmd("renamechar %s %s" % (self.rename_old, new))
        self.rename_old = None
        self.field = None
        self.msg = "已发送改名请求"
        time.sleep(0.3)
        self.cmd("charlist")

    def back_login(self):
        if self.client:
            try:
                self.client.sock.close()
            except Exception:
                pass
        self.client = None
        self.token = ""
        self.state = None
        self.screen_name = "login"
        self.msg = ""

    def poll(self):
        if not self.client:
            return
        self.client.poll()
        if not getattr(self.client, "connected", True):
            self.back_login()
            self.msg = "与服务器断开连接，请重新登录。"
            return
        now = time.time()
        st = self.state
        if st:
            if st.level > self.last_level:
                if self.last_level:
                    self.play("level")
                self.last_level = st.level
            if len(st.effects) > self.last_effects:
                if any(e.get("kind") == "damage" for e in st.effects):
                    self.play("hit")
            self.last_effects = len(st.effects)
        if now - self.last_social > 2:
            self.last_social = now
            for c in ("mapdata", "questlog", "partylist", "guildinfo", "dungeonlist"):
                self.cmd(c)
        if self.state and self.state.trade and self.panel != "交易":
            self.panel = "交易"

    def draw_world(self):
        st = self.state
        if not st:
            return
        self.draw_bg()
        # 相机平滑
        if not self.cam_ready:
            self.cam = [st.x, st.y]; self.cam_ready = True
        self.cam[0] += (st.x - self.cam[0]) * 0.25
        self.cam[1] += (st.y - self.cam[1]) * 0.25
        bx, by = int(self.cam[0]), int(self.cam[1])
        title = st.map_title or st.map_name or ""
        dungeon = any(k in title for k in ("殿", "谷", "寺", "坟", "洞", "地", "刹", "矿", "阁", "魔龙"))
        for sy in range(-18, 19):
            for sx in range(-18, 19):
                wx, wy = bx + sx, by + sy
                px = int(W / 2 + ((wx - self.cam[0]) - (wy - self.cam[1])) * TW / 2)
                py = int(H / 2 + 40 + ((wx - self.cam[0]) + (wy - self.cam[1])) * TH / 2)
                if px < -TW or px > W + TW or py < -TH or py > H + TH:
                    continue
                block = self.terrain_at(wx, wy)
                if block != 2:
                    img = self.floor.get("crypt" if dungeon else ("grass" if (wx * 7 + wy * 13) % 3 == 0 else "grass2"))
                    if img:
                        screen.blit(img, (px - TW // 2, py - TH // 2))
                    else:
                        pts = [(px, py - TH // 2), (px + TW // 2, py), (px, py + TH // 2), (px - TW // 2, py)]
                        pygame.draw.polygon(screen, (45, 70, 50), pts)
                if block == 1 and self.floor.get("wall"):
                    screen.blit(self.floor["wall"], (px - TW // 2, py + TH // 2 - 64))
        ents = sorted(st.entities.values(), key=lambda e: e["x"] + e["y"])
        now = time.time()
        for e in ents:
            px, py = self.iso(e["x"], e["y"])
            key = e["id"]
            last = self.anim_last.get(key)
            moving = False
            if last is None or last[0] != e["x"] or last[1] != e["y"]:
                self.anim_last[key] = [e["x"], e["y"], now + 0.26]
            else:
                moving = last[2] > now
            frames = self.sprite_for(e)
            idx = 0
            if moving and frames and len(frames) > 2:
                idx = 1 + int(now * 7) % 2
            img = frames[idx] if frames and idx < len(frames) else (frames[0] if frames else None)
            if img:
                screen.blit(img, (px - 32, py + 10 - 64))
            else:
                pygame.draw.rect(screen, (160, 70, 50), (px - 10, py - 20, 20, 30))
            nm = text(e["name"][:6], 12, PALE)
            screen.blit(nm, (px - nm.get_width() // 2, py + 10))
            if e["type"] == 3 and e.get("maxhp", 0) > 0:
                r = max(0, min(1, e.get("hp", 0) / e["maxhp"]))
                pygame.draw.rect(screen, (60, 20, 15), (px - 14, py - 58, 28, 4))
                pygame.draw.rect(screen, (200, 70, 55), (px - 14, py - 58, int(28 * r), 4))
        own = self.sprite_for({"type": 5})
        ox, oy = W // 2, H // 2 + 40
        own_until = self.own_attack_until
        if self.own_last is None or self.own_last != (st.x, st.y):
            self.own_last = (st.x, st.y)
            self.own_move_until = time.time() + 0.26
        own_idx = 0
        if time.time() < own_until:
            own_idx = 2
        elif getattr(self, "own_move_until", 0) > time.time() and own and len(own) > 2:
            own_idx = 1 + int(time.time() * 7) % 2
        if own and own[own_idx]:
            screen.blit(own[own_idx], (ox - 32, oy + 10 - 64))
        nm = text(st.name, 13, (245, 220, 150))
        screen.blit(nm, (ox - nm.get_width() // 2, oy + 12))
        self.draw_hud()

    def bar(self, x, y, w, h, ratio, color):
        pygame.draw.rect(screen, (30, 22, 14), (x, y, w, h))
        pygame.draw.rect(screen, color, (x, y, max(0, int(w * min(1, max(0, ratio)))), h))

    def draw_hud(self):
        st = self.state
        panel = pygame.Rect(10, 10, 330, 150)
        panel996(panel)
        orb = pygame.Rect(28, 28, 74, 74)
        if self.ui["orb"]:
            screen.blit(pygame.transform.smoothscale(self.ui["orb"], orb.size), orb.topleft)
        cls_fn = "warrior.gif" if getattr(st, "player_class", 0) == 0 else (
            "mage.gif" if st.player_class == 1 else "taoist.gif")
        fr = self.frames.get(cls_fn) or [self.sprites.get(cls_fn)] * 3
        if fr and fr[0]:
            por = pygame.transform.smoothscale(fr[0], (48, 48))
            screen.blit(por, (orb.x + 13, orb.y + 10))
        screen.blit(text(st.name, 16, PALE), (118, 30))
        screen.blit(text("Lv.%d  %s" % (st.level, ("战士", "法师", "道士")[getattr(st, "player_class", 0)]), 12, GOLD), (118, 54))
        self.bar(118, 76, 190, 10, st.hp / max(1, st.maxhp), (180, 55, 45))
        self.bar(118, 92, 190, 10, st.mp / max(1, st.maxmp), (55, 85, 180))
        self.bar(118, 108, 190, 8, st.exp / max(1, st.maxexp), (200, 160, 60))
        screen.blit(text("金币 %d   %d,%d" % (st.gold, st.x, st.y), 11, GOLD), (118, 126))
        # 小地图
        mp = pygame.Rect(W - 205, 10, 195, 150)
        panel996(mp)
        self.draw_minimap(pygame.Rect(W - 195, 18, 175, 115))
        screen.blit(text(st.map_title or st.map_name, 12, GOLD), (W - 150, 138))
        # 聊天
        chat = pygame.Rect(10, H - 185, 440, 170)
        panel996(chat)
        lines = st.chat[-6:]
        for i, line in enumerate(lines):
            screen.blit(text(line[:52], 12, PALE), (22, chat.y + 12 + i * 18))
        inp = pygame.Rect(22, chat.bottom - 30, 410, 22)
        pygame.draw.rect(screen, (15, 12, 8), inp)
        pygame.draw.rect(screen, GOLD if self.chat_active else (90, 70, 40), inp, 1)
        screen.blit(text(self.chat_input or "回车输入聊天 / 命令", 12, PALE), (inp.x + 6, inp.y + 3))
        # 任务追踪
        active = [q for q in (st.quests or []) if q.get("state") == 1]
        if active:
            qr = pygame.Rect(W - 205, 170, 195, 110)
            panel996(qr)
            screen.blit(text("任务追踪", 12, GOLD), (qr.x + 10, qr.y + 8))
            for i, q in enumerate(active[:3]):
                screen.blit(text("%s %d/%d" % (q["name"], q["progress"], q["need"]), 11, PALE),
                            (qr.x + 10, qr.y + 30 + i * 20))
        # 技能栏
        sx = 480
        for i, sk in enumerate((st.skills or [])[:8]):
            r = pygame.Rect(sx + i * 40, H - 62, 38, 42)
            pygame.draw.rect(screen, (30, 22, 12), r)
            pygame.draw.rect(screen, GOLD, r, 1)
            screen.blit(text("F%d" % (i + 1), 9, GOLD), (r.x + 3, r.y + 1))
            screen.blit(text(sk["name"][:4], 9, PALE), (r.x + 2, r.y + 16))
            left = self.cd.get(sk["name"], 0) - time.time()
            if left > 0:
                ov = pygame.Surface((r.w, r.h), pygame.SRCALPHA)
                ov.fill((0, 0, 0, 150))
                screen.blit(ov, r.topleft)
                screen.blit(text("%.1f" % left, 10, (255, 255, 255)), (r.x + 10, r.y + 12))
        # 菜单
        names = [("角色", "C"), ("背包", "B"), ("技能", "K"), ("任务", "Q"),
                 ("队伍", "P"), ("副本", "D"), ("交易", "T"), ("商店", "V"),
                 ("行会", "G"), ("系统", "Esc")]
        for i, (nm, key) in enumerate(names):
            r = pygame.Rect(sx + i * 52, H - 24, 49, 20)
            pygame.draw.rect(screen, (35, 26, 14), r)
            pygame.draw.rect(screen, (110, 85, 40), r, 1)
            screen.blit(text(nm, 10, PALE), (r.x + 4, r.y + 3))
            self.buttons.append((r, lambda k=nm: self.open_panel(k)))

    def draw_minimap(self, rect):
        pygame.draw.rect(screen, (10, 9, 6), rect)
        t = self.state.terrain if self.state else None
        if not t:
            return
        sc = min(rect.w / t["w"], rect.h / t["h"])
        ox = rect.x + (rect.w - t["w"] * sc) / 2
        oy = rect.y + (rect.h - t["h"] * sc) / 2
        for y in range(0, t["h"], 2):
            for x in range(0, t["w"], 2):
                b = t["blocks"][y * t["w"] + x]
                if b == 2:
                    continue
                color = (65, 55, 45) if b == 1 else (50, 80, 55)
                pygame.draw.rect(screen, color, (ox + x * sc, oy + y * sc, max(1, 2 * sc), max(1, 2 * sc)))
        for e in self.state.entities.values():
            color = (210, 90, 70) if e["type"] == 3 else (220, 190, 100)
            pygame.draw.circle(screen, color, (int(ox + (e["x"] - t["ox"]) * sc), int(oy + (e["y"] - t["oy"]) * sc)), 2)
        pygame.draw.circle(screen, (255, 255, 255),
                           (int(ox + (self.state.x - t["ox"]) * sc), int(oy + (self.state.y - t["oy"]) * sc)), 3)

    def open_panel(self, name):
        self.panel = name
        cmds = {"队伍": "partylist", "行会": "guildinfo", "副本": "dungeonlist",
                "技能": "skills", "任务": "questlog", "商店": "shop", "背包": "bag",
                "角色": "equipment"}
        if name in cmds:
            self.cmd(cmds[name])

    def draw_panel(self):
        if not self.panel:
            return
        rect = pygame.Rect(W // 2 - 300, 90, 600, 500)
        panel996(rect, self.panel)
        close = pygame.Rect(rect.right - 40, rect.y + 16, 26, 26)
        pygame.draw.rect(screen, (90, 40, 30), close)
        screen.blit(text("×", 18, PALE), (close.x + 6, close.y + 1))
        self.buttons.append((close, lambda: setattr(self, "panel", None)))
        if self.panel == "背包":
            self.draw_bag(rect)
        elif self.panel == "技能":
            self.draw_skills(rect)
        elif self.panel == "任务":
            self.draw_quests(rect)
        elif self.panel == "商店":
            self.draw_shop(rect)
        elif self.panel == "队伍":
            self.draw_party(rect)
        elif self.panel == "行会":
            self.draw_guild(rect)
        elif self.panel == "副本":
            self.draw_dungeon(rect)
        elif self.panel == "交易":
            self.draw_trade(rect)
        elif self.panel == "角色":
            self.draw_char(rect)
        elif self.panel == "系统":
            self.draw_system(rect)
        if self.tooltip:
            tx, ty, msg = self.tooltip
            im = text(msg, 12, PALE)
            box = pygame.Rect(tx, ty, im.get_width() + 16, 26)
            pygame.draw.rect(screen, (15, 11, 7), box)
            pygame.draw.rect(screen, GOLD, box, 1)
            screen.blit(im, (box.x + 8, box.y + 6))

    def rows(self, rect, items, start=70, line=28):
        y = rect.y + start
        for it in items:
            screen.blit(text(it, 12, PALE), (rect.x + 30, y))
            y += line
        return y

    def draw_bag(self, rect):
        bag = self.state.bag or []
        y = rect.y + 66
        for i, it in enumerate(bag[:14]):
            x = rect.x + 30 + (i % 2) * 260
            yy = y + (i // 2) * 40
            r = pygame.Rect(x, yy, 240, 34)
            pygame.draw.rect(screen, (20, 15, 9), r); pygame.draw.rect(screen, (90, 70, 35), r, 1)
            ic = self.icon_for(it["name"])
            if ic:
                screen.blit(ic, (r.x + 4, r.y + 3))
            screen.blit(text("%s x%d" % (it["name"], it["count"]), 12, PALE), (r.x + 38, r.y + 8))
            if r.collidepoint(pygame.mouse.get_pos()):
                self.tooltip = (r.x + 20, r.y + 20, "%s  x%d" % (it["name"], it["count"]))
            self.buttons.append((r, lambda i=it["id"]: self.cmd("use %d" % i if not self.bag_equip(it) else "equipauto %d" % i)))

    def bag_equip(self, it):
        return not (it.get("usetype") or it["name"].endswith("药") or "书" in it["name"])

    def draw_skills(self, rect):
        y = rect.y + 66
        for i, sk in enumerate(self.state.skills or []):
            screen.blit(text("F%d %s  Lv%d %s" % (i + 1, sk["name"], sk["need"],
                                                 "已学" if sk["learned"] else "未学"), 12, PALE),
                        (rect.x + 30, y))
            if sk["learned"]:
                b = pygame.Rect(rect.right - 110, y - 3, 70, 22)
                pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
                screen.blit(text("施放", 11, PALE), (b.x + 20, b.y + 3))
                self.buttons.append((b, lambda n=sk["name"]: self.cast_skill(n)))
            y += 28

    def draw_quests(self, rect):
        y = rect.y + 66
        for q in (self.state.quests or [])[:10]:
            state = ("可接", "进行中", "已完成")[q["state"]] if q["state"] < 3 else ""
            screen.blit(text("[%s] %s %d/%d" % (state, q["name"], q["progress"], q["need"]), 12, PALE),
                        (rect.x + 30, y))
            b = pygame.Rect(rect.right - 110, y - 3, 70, 22)
            if q["state"] == 0:
                pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
                screen.blit(text("接取", 11, PALE), (b.x + 20, b.y + 3))
                self.buttons.append((b, lambda n=q["name"]: self.cmd("accept " + n)))
            elif q["state"] == 1 and q["progress"] >= q["need"]:
                pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
                screen.blit(text("提交", 11, PALE), (b.x + 20, b.y + 3))
                self.buttons.append((b, lambda n=q["name"]: self.cmd("submit " + n)))
            y += 30

    def draw_shop(self, rect):
        y = rect.y + 66
        for it in (self.state.shop or [])[:14]:
            ic = self.icon_for(it["name"])
            if ic:
                screen.blit(ic, (rect.x + 30, y - 2))
            screen.blit(text("%s  %d 金币" % (it["name"], it["price"]), 12, PALE), (rect.x + 64, y))
            if pygame.Rect(rect.x + 24, y - 6, 300, 26).collidepoint(pygame.mouse.get_pos()):
                self.tooltip = (rect.x + 80, y + 10, "%s  %d 金币" % (it["name"], it["price"]))
            b = pygame.Rect(rect.right - 100, y - 3, 60, 22)
            pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
            screen.blit(text("购买", 11, PALE), (b.x + 12, b.y + 3))
            self.buttons.append((b, lambda n=it["name"]: self.cmd("buy %s 1" % n)))
            y += 26

    def draw_party(self, rect):
        y = rect.y + 66
        for i, m in enumerate(self.state.party or []):
            screen.blit(text(("队长 " if i == 0 else "") + "%s Lv.%d %d/%d" %
                             (m["name"], m["level"], m["hp"], m["maxhp"]), 12, PALE), (rect.x + 30, y))
            y += 26
        if not self.state.party:
            screen.blit(text("还没有队伍", 12, (170, 150, 120)), (rect.x + 30, y))
        self.field_box(pygame.Rect(rect.x + 30, rect.bottom - 60, 180, 28), "party_invite", "玩家名")
        b = pygame.Rect(rect.x + 220, rect.bottom - 60, 70, 28)
        pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
        screen.blit(text("邀请", 11, PALE), (b.x + 20, b.y + 6))
        self.buttons.append((b, lambda: self.cmd("partyinvite " + self.cmd_field("party_invite"))))
        b2 = pygame.Rect(rect.x + 300, rect.bottom - 60, 70, 28)
        pygame.draw.rect(screen, (55, 38, 18), b2); pygame.draw.rect(screen, GOLD, b2, 1)
        screen.blit(text("离队", 11, PALE), (b2.x + 20, b2.y + 6))
        self.buttons.append((b2, lambda: self.cmd("partyleave")))

    def draw_guild(self, rect):
        g = self.state.guild
        if not g:
            screen.blit(text("尚未加入行会（创建需 1000 金币）", 12, PALE), (rect.x + 30, rect.y + 70))
            self.field_box(pygame.Rect(rect.x + 30, rect.y + 110, 200, 28), "guild_name", "新行会名")
            b = pygame.Rect(rect.x + 240, rect.y + 110, 60, 28)
            pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
            screen.blit(text("创建", 11, PALE), (b.x + 16, b.y + 6))
            self.buttons.append((b, lambda: self.cmd("guild create " + self.cmd_field("guild_name"))))
            self.field_box(pygame.Rect(rect.x + 30, rect.y + 155, 200, 28), "guild_join", "已有行会名")
            b2 = pygame.Rect(rect.x + 240, rect.y + 155, 60, 28)
            pygame.draw.rect(screen, (55, 38, 18), b2); pygame.draw.rect(screen, GOLD, b2, 1)
            screen.blit(text("加入", 11, PALE), (b2.x + 16, b2.y + 6))
            self.buttons.append((b2, lambda: self.cmd("guild join " + self.cmd_field("guild_join"))))
            return
        screen.blit(text("行会：%s  Lv.%d  资金 %d" % (g["name"], g["level"], g["funds"]), 13, PALE),
                    (rect.x + 30, rect.y + 66))
        y = rect.y + 100
        for m in g["members"][:10]:
            rank = "会长 " if m["rank"] == 2 else ("副会长 " if m["rank"] == 1 else "")
            screen.blit(text("%s%s Lv.%d" % (rank, m["name"], m["level"]), 12, PALE), (rect.x + 30, y))
            y += 26
        self.field_box(pygame.Rect(rect.x + 30, rect.bottom - 60, 160, 28), "guild_donate", "捐献金币")
        b = pygame.Rect(rect.x + 200, rect.bottom - 60, 60, 28)
        pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
        screen.blit(text("捐献", 11, PALE), (b.x + 16, b.y + 6))
        self.buttons.append((b, lambda: self.cmd("guild donate " + (self.cmd_field("guild_donate") or "0"))))
        b2 = pygame.Rect(rect.x + 270, rect.bottom - 60, 60, 28)
        pygame.draw.rect(screen, (55, 38, 18), b2); pygame.draw.rect(screen, GOLD, b2, 1)
        screen.blit(text("退会", 11, PALE), (b2.x + 16, b2.y + 6))
        self.buttons.append((b2, lambda: self.cmd("guild leave")))

    def draw_dungeon(self, rect):
        y = rect.y + 66
        for d in self.state.dungeons or []:
            screen.blit(text("%s  Boss:%s  %s" % (d["name"], d["boss"], "已首通" if d["first"] else "未首通"),
                             12, PALE), (rect.x + 30, y))
            y += 28
        if self.state.dungeon_records:
            screen.blit(text("排行榜", 13, GOLD), (rect.x + 30, y + 8))
            y += 34
            for r in self.state.dungeon_records[:4]:
                screen.blit(text("%s %s %ds" % (r["dungeon"], r["party"], r["seconds"]), 11, PALE),
                            (rect.x + 30, y))
                y += 24

    def draw_char(self, rect):
        eq = self.state.equip or []
        names = {0: "武器", 1: "衣服", 2: "头盔", 3: "项链", 4: "戒指", 5: "戒指",
                 6: "手镯", 7: "手镯", 8: "腰带", 9: "靴子"}
        y = rect.y + 66
        for it in eq:
            ic = self.icon_for(it["name"])
            if ic:
                screen.blit(ic, (rect.x + 30, y - 2))
            screen.blit(text("%s：%s" % (names.get(it["pos"], "装备"), it["name"]), 12, PALE),
                        (rect.x + 64, y))
            y += 26
        screen.blit(text("攻击 %d-%d  防御 %d-%d  魔法 %d-%d  道术 %d-%d" %
                         (self.state.min_atk, self.state.max_atk, self.state.min_def, self.state.max_def,
                          self.state.min_mc, self.state.max_mc, self.state.min_sc, self.state.max_sc),
                         12, GOLD), (rect.x + 30, y + 10))

    def draw_trade(self, rect):
        t = self.state.trade
        if not t:
            screen.blit(text("输入附近玩家名发起交易（同地图 6 格内）", 12, PALE), (rect.x + 30, rect.y + 70))
            self.field_box(pygame.Rect(rect.x + 30, rect.y + 110, 220, 28), "trade_target", "玩家名")
            b = pygame.Rect(rect.x + 260, rect.y + 110, 80, 28)
            pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
            screen.blit(text("发起交易", 11, PALE), (b.x + 12, b.y + 6))
            self.buttons.append((b, lambda: self.cmd("trade " + self.cmd_field("trade_target"))))
            return
        screen.blit(text("对方：%s  我的金币 %d  对方金币 %d" % (t["partner"], t["my_gold"], t["other_gold"]),
                         13, PALE), (rect.x + 30, rect.y + 66))
        y = rect.y + 100
        screen.blit(text("我的物品：" + ", ".join("%s x%d" % (i["name"], i["count"]) for i in t["my_items"]), 11, PALE),
                    (rect.x + 30, y)); y += 30
        screen.blit(text("对方物品：" + ", ".join("%s x%d" % (i["name"], i["count"]) for i in t["other_items"]), 11, PALE),
                    (rect.x + 30, y))
        # 背包快捷放入
        bag = self.state.bag or []
        bx = rect.x + 30; by = rect.bottom - 140
        for i, it in enumerate(bag[:8]):
            b = pygame.Rect(bx + (i % 4) * 130, by + (i // 4) * 26, 124, 22)
            pygame.draw.rect(screen, (35, 27, 16), b); pygame.draw.rect(screen, (110, 85, 40), b, 1)
            screen.blit(text("放入 %s" % it["name"][:5], 10, PALE), (b.x + 5, b.y + 3))
            self.buttons.append((b, lambda i=it["id"]: self.cmd("trade add %d" % i)))
        # 金币
        self.field_box(pygame.Rect(rect.x + 30, rect.bottom - 78, 120, 22), "trade_gold", "金币数")
        b = pygame.Rect(rect.x + 158, rect.bottom - 78, 60, 22)
        pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
        screen.blit(text("设置", 10, PALE), (b.x + 16, b.y + 3))
        self.buttons.append((b, lambda: self.cmd("trade gold " + (self.cmd_field("trade_gold") or "0"))))
        for lab, cmd in (("锁定", "trade lock"), ("确认", "trade confirm"), ("取消", "trade cancel")):
            b = pygame.Rect(rect.right - 250 + (("锁定", "确认", "取消").index(lab)) * 80, rect.bottom - 78, 74, 22)
            pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
            screen.blit(text(lab, 11, PALE), (b.x + 22, b.y + 3))
            self.buttons.append((b, lambda c=cmd: self.cmd(c)))

    def draw_system(self, rect):
        screen.blit(text("账号：%s" % self.account, 12, PALE), (rect.x + 30, rect.y + 70))
        b = pygame.Rect(rect.x + 30, rect.bottom - 60, 120, 30)
        pygame.draw.rect(screen, (55, 38, 18), b); pygame.draw.rect(screen, GOLD, b, 1)
        screen.blit(text("退出登录", 12, PALE), (b.x + 24, b.y + 6))
        self.buttons.append((b, self.back_login))

    # ---------------------------------------------------------------- input
    def world_click(self, pos, right=False):
        st = self.state
        if not st:
            return
        mx, my = pos
        a = (mx - W / 2) / (TW / 2)
        b = (my - (H / 2 + 40)) / (TH / 2)
        dx = (a + b) / 2
        dy = (b - a) / 2
        tx, ty = round(st.x + dx), round(st.y + dy)
        ent = next((e for e in st.entities.values() if e["x"] == tx and e["y"] == ty), None)
        d = max(abs(tx - st.x), abs(ty - st.y))
        self.target = None
        self.move_goal = None
        if ent and ent["type"] == 3:
            if d <= 1:
                self.cmd("attack " + str(self.dir_to(tx - st.x, ty - st.y)))
            else:
                self.target = ent["id"]
            return
        if ent and ent["type"] == 4:
            if d <= 3:
                self.cmd("talk " + ent["name"])
            else:
                self.move_goal = (ent["x"], ent["y"])
            return
        if d == 0:
            self.cmd("pickup")
        else:
            self.move_goal = (tx, ty)

    def dir_to(self, dx, dy):
        sx, sy = (1 if dx > 0 else (-1 if dx < 0 else 0)), (1 if dy > 0 else (-1 if dy < 0 else 0))
        return {(0, -1): 0, (1, -1): 1, (1, 0): 2, (1, 1): 3, (0, 1): 4, (-1, 1): 5, (-1, 0): 6, (-1, -1): 7}.get((sx, sy), 4)

    def autopilot(self):
        st = self.state
        if not st:
            return
        now = time.time()
        if self.target:
            ent = st.entities.get(self.target)
            if not ent:
                self.target = None; return
            if max(abs(ent["x"] - st.x), abs(ent["y"] - st.y)) <= 1:
                if now - self.last_attack > 0.9:
                    self.cmd("attack " + str(self.dir_to(ent["x"] - st.x, ent["y"] - st.y)))
                    self.last_attack = now
                return
            self.move_goal = (ent["x"], ent["y"])
        if self.move_goal and now - self.last_step > 0.32:
            gx, gy = self.move_goal
            if (st.x, st.y) == (gx, gy):
                self.move_goal = None; return
            dx, dy = gx - st.x, gy - st.y
            d = self.dir_to(dx, dy)
            self.cmd("walk " + str(d))
            self.last_step = now

    def draw_dialog(self):
        d = self.state.dialog if self.state else None
        if not d:
            return
        key = self.dialog_key(d)
        if key == self.dialog_dismissed:
            return
        box = pygame.Rect(W // 2 - 280, H - 350, 560, 230)
        if self.ui["wide"]:
            screen.blit(pygame.transform.smoothscale(self.ui["wide"], box.size), box.topleft)
        else:
            pygame.draw.rect(screen, (38, 28, 16), box); pygame.draw.rect(screen, GOLD, box, 2)
        screen.blit(text(d["npc"], 18, PALE), (box.x + 24, box.y + 18))
        screen.blit(text(d["text"][:42], 13, PALE), (box.x + 24, box.y + 52))
        y = box.y + 84
        for opt in d.get("options", []):
            b = pygame.Rect(box.x + 24, y, box.w - 100, 28)
            pygame.draw.rect(screen, (25, 18, 10), b)
            pygame.draw.rect(screen, (110, 85, 40), b, 1)
            screen.blit(text("%d. %s" % (opt["index"], opt["text"]), 12, PALE), (b.x + 10, b.y + 5))
            self.buttons.append((b, lambda o=opt: self.choose_dialog(o)))
            y += 30
        close = pygame.Rect(box.right - 42, box.y + 14, 26, 26)
        pygame.draw.rect(screen, (90, 40, 30), close)
        screen.blit(text("×", 18, PALE), (close.x + 6, close.y + 1))
        self.buttons.append((close, lambda: setattr(self, "dialog_dismissed", key)))
        self.dialog_dismissed_key = key

    def choose_dialog(self, opt):
        key = getattr(self, "dialog_dismissed_key", "")
        self.dialog_dismissed = key
        action = opt.get("action", "")
        if action == "shop":
            self.open_panel("商店"); self.cmd("shop")
        elif action == "quests":
            self.open_panel("任务"); self.cmd("questlog")
        elif action == "close":
            pass
        else:
            self.cmd("choose %s %d" % (self.state.dialog["npc"], opt["index"]))

    def draw_effects(self):
        now = time.time()
        for ef in (self.state.effects if self.state else []):
            left = ef.get("until", now) - now
            if left <= 0:
                continue
            ex, ey = self.iso(ef["x"], ef["y"])
            if ef.get("kind") == "damage":
                screen.blit(text(ef.get("text", ""), 18, (255, 90, 80)), (ex - 10, ey - 80 - int((1 - left / 0.8) * 24)))
            else:
                r = int(12 + (1 - left / 0.5) * 30)
                pygame.draw.circle(screen, (110, 190, 255), (ex, ey - 16), max(4, r), 2)

    def draw_target(self):
        if not self.target or not self.state:
            return
        e = self.state.entities.get(self.target)
        if not e:
            return
        box = pygame.Rect(W // 2 - 130, 12, 260, 44)
        if self.ui["panel"]:
            screen.blit(pygame.transform.smoothscale(self.ui["panel"], box.size), box.topleft)
        else:
            pygame.draw.rect(screen, (38, 28, 16), box); pygame.draw.rect(screen, GOLD, box, 1)
        screen.blit(text(e["name"], 13, PALE), (box.x + 10, box.y + 5))
        if e.get("maxhp", 0) > 0:
            self.bar(box.x + 10, box.y + 28, box.w - 20, 7, e.get("hp", 0) / e["maxhp"], (190, 60, 50))

    def handle(self, ev):
        if ev.type == pygame.QUIT:
            return False
        if ev.type == pygame.MOUSEBUTTONDOWN:
            pos = pygame.mouse.get_pos()
            for r, cb in reversed(self.buttons):
                if r.collidepoint(pos):
                    cb(); return True
            if self.screen_name == "game" and not self.panel:
                self.world_click(pos, ev.button == 3)
        elif ev.type == pygame.KEYDOWN:
            if self.field:
                if ev.key in (pygame.K_RETURN, pygame.K_ESCAPE):
                    self.field = None
                elif ev.key == pygame.K_BACKSPACE:
                    self.inputs[self.field] = self.inputs.get(self.field, "")[:-1]
                elif ev.unicode and ev.unicode.isprintable():
                    self.inputs[self.field] = self.inputs.get(self.field, "") + ev.unicode
                return True
            if self.chat_active:
                if ev.key == pygame.K_RETURN:
                    if self.chat_input.strip():
                        t = self.chat_input.strip()
                        self.cmd(t[1:] if t.startswith("/") else "say " + t)
                    self.chat_input = ""; self.chat_active = False
                elif ev.key == pygame.K_ESCAPE:
                    self.chat_input = ""; self.chat_active = False
                elif ev.key == pygame.K_BACKSPACE:
                    self.chat_input = self.chat_input[:-1]
                elif ev.unicode and ev.unicode.isprintable():
                    self.chat_input += ev.unicode
                return True
            if self.active == "account" or self.active == "password" or self.active == "newchar":
                if ev.key == pygame.K_RETURN:
                    if self.screen_name == "login":
                        self.login(False)
                    elif self.active == "newchar":
                        self.create_char()
                    self.active = None
                elif ev.key == pygame.K_BACKSPACE:
                    if self.active == "account": self.account = self.account[:-1]
                    elif self.active == "password": self.password = self.password[:-1]
                    else: self.new_char = self.new_char[:-1]
                elif ev.key == pygame.K_TAB:
                    self.active = "password" if self.active == "account" else "account"
                elif ev.unicode and ev.unicode.isprintable():
                    if self.active == "account": self.account += ev.unicode
                    elif self.active == "password": self.password += ev.unicode
                    else: self.new_char += ev.unicode
                return True
            if ev.key == pygame.K_RETURN:
                self.chat_active = True
            elif ev.key == pygame.K_ESCAPE:
                self.panel = None
            elif ev.key in (pygame.K_F1, pygame.K_F2, pygame.K_F3, pygame.K_F4, pygame.K_F5, pygame.K_F6, pygame.K_F7, pygame.K_F8):
                idx = ev.key - pygame.K_F1
                if self.state and idx < len(self.state.skills or []):
                    self.cast_skill(self.state.skills[idx]["name"])
            elif pygame.K_1 <= ev.key <= pygame.K_6:
                idx = ev.key - pygame.K_1
                pots = [i for i in (self.state.bag if self.state else []) if "药" in i["name"]]
                if idx < len(pots): self.cmd("use %d" % pots[idx]["id"])
            elif ev.key == pygame.K_b: self.open_panel("背包")
            elif ev.key == pygame.K_c: self.open_panel("角色")
            elif ev.key == pygame.K_k: self.open_panel("技能")
            elif ev.key == pygame.K_q: self.open_panel("任务")
            elif ev.key == pygame.K_v: self.open_panel("商店")
            elif ev.key == pygame.K_g: self.open_panel("行会")
            elif ev.key == pygame.K_p: self.open_panel("队伍")
            elif ev.key == pygame.K_d: self.open_panel("副本")
            elif ev.key == pygame.K_t: self.open_panel("交易")
        return True

    def run(self):
        running = True
        self.new_char = ""
        self.new_class = 0
        while running:
            for ev in pygame.event.get():
                running = self.handle(ev) and running
            if self.client:
                self.poll()
                self.autopilot()
            self.buttons = []
            self.tooltip = None
            if self.screen_name == "login":
                self.draw_login()
            elif self.screen_name == "chars":
                self.draw_chars()
            else:
                self.draw_world()
                self.draw_target()
                self.draw_effects()
                self.draw_panel()
                self.draw_dialog()
            pygame.display.flip()
            clock.tick(45)
        pygame.quit()

if __name__ == "__main__":
    d = Desktop()
    if "--chars" in sys.argv:
        d.account = "网页演示"
        d.password = "123456"
        d.login(False)
        import time as _t
        _t.sleep(0.6)
        d.client.poll()
        d.run()
        sys.exit(0)
    if "--auto" in sys.argv:
        d.account = "网页演示"
        d.password = "123456"
        d.login(False)
        import time as _t
        end = _t.time() + 3
        while _t.time() < end and not (d.state and d.state.characters):
            d.client.poll()
            _t.sleep(0.05)
        d.cmd("charlist")
        _t.sleep(0.5)
        d.client.poll()
        if d.state and d.state.characters:
            d.select_char(d.state.characters[0]["name"])
    d.run()
