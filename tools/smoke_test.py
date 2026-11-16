#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GameServer 骨架端到端冒烟测试（传奇世界玩法版）。

覆盖：启动加载（地图/物品/职业/技能/怪物）、创建职业角色、发物品、装备、
      战斗击杀、升级成长、技能释放、存档、重启读档、死亡复活。

用法：
    python3 tools/smoke_test.py
    make smoke
"""
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "GameServer")
NAME = "冒烟角色"
SAVE = os.path.join(ROOT, "data", "players", NAME + ".dat")
SAVE2 = os.path.join(ROOT, "data", "players", "法测.dat")
SAVE3 = os.path.join(ROOT, "data", "players", "商测.dat")
SAVE4 = os.path.join(ROOT, "data", "players", "回测.dat")
SAVE5 = os.path.join(ROOT, "data", "players", "NPC测.dat")
SAVE6 = os.path.join(ROOT, "data", "players", "堆测.dat")
SAVE7 = os.path.join(ROOT, "data", "players", "书测.dat")
SAVE8 = os.path.join(ROOT, "data", "players", "任测.dat")
SAVE9 = os.path.join(ROOT, "data", "players", "强测.dat")
SAVE10 = os.path.join(ROOT, "data", "players", "队甲.dat")
SAVE11 = os.path.join(ROOT, "data", "players", "队乙.dat")
SAVE12 = os.path.join(ROOT, "data", "players", "会甲.dat")
SAVE13 = os.path.join(ROOT, "data", "players", "会乙.dat")
SAVE14 = os.path.join(ROOT, "data", "players", "图测.dat")
SAVE15 = os.path.join(ROOT, "data", "players", "链测.dat")
SAVE16 = os.path.join(ROOT, "data", "players", "谈测.dat")
SAVE17 = os.path.join(ROOT, "data", "players", "会测.dat")
SAVE18 = os.path.join(ROOT, "data", "players", "会测2.dat")
SAVE19 = os.path.join(ROOT, "data", "players", "仓测.dat")
SAVE20 = os.path.join(ROOT, "data", "players", "权测.dat")
SAVE21 = os.path.join(ROOT, "data", "players", "权测2.dat")
SAVE22 = os.path.join(ROOT, "data", "players", "寺测.dat")
SAVE23 = os.path.join(ROOT, "data", "players", "新图测.dat")
SAVE24 = os.path.join(ROOT, "data", "players", "套测.dat")
SAVE25 = os.path.join(ROOT, "data", "players", "附测.dat")
SAVE26 = os.path.join(ROOT, "data", "players", "世测.dat")
SAVE27 = os.path.join(ROOT, "data", "players", "副甲.dat")
SAVE28 = os.path.join(ROOT, "data", "players", "副乙.dat")
SAVE29 = os.path.join(ROOT, "data", "players", "榜甲.dat")
SAVE30 = os.path.join(ROOT, "data", "players", "榜乙.dat")
SAVE31 = os.path.join(ROOT, "data", "players", "并甲.dat")
SAVE32 = os.path.join(ROOT, "data", "players", "并乙.dat")
SAVE33 = os.path.join(ROOT, "data", "players", "并丙.dat")
SAVE34 = os.path.join(ROOT, "data", "players", "并丁.dat")
SAVE35 = os.path.join(ROOT, "data", "players", "重甲.dat")
SAVE36 = os.path.join(ROOT, "data", "players", "重乙.dat")
SAVE37 = os.path.join(ROOT, "data", "players", "成甲.dat")
SAVE38 = os.path.join(ROOT, "data", "players", "成乙.dat")
SAVE39 = os.path.join(ROOT, "data", "players", "点甲.dat")
SAVE40 = os.path.join(ROOT, "data", "players", "尸甲.dat")
SAVE41 = os.path.join(ROOT, "data", "players", "尸乙.dat")
SAVE42 = os.path.join(ROOT, "data", "players", "蛇甲.dat")
SAVE43 = os.path.join(ROOT, "data", "players", "蛇乙.dat")
SAVE44 = os.path.join(ROOT, "data", "players", "矿甲.dat")
SAVE45 = os.path.join(ROOT, "data", "players", "矿乙.dat")
SAVE46 = os.path.join(ROOT, "data", "players", "坟甲.dat")
SAVE47 = os.path.join(ROOT, "data", "players", "坟乙.dat")
SAVE48 = os.path.join(ROOT, "data", "players", "套甲.dat")
SAVE49 = os.path.join(ROOT, "data", "players", "祖甲.dat")
SAVE50 = os.path.join(ROOT, "data", "players", "祖乙.dat")
SAVE51 = os.path.join(ROOT, "data", "players", "图甲.dat")
SAVE52 = os.path.join(ROOT, "data", "players", "集甲.dat")
SAVE53 = os.path.join(ROOT, "data", "players", "鉴甲.dat")
SAVE54 = os.path.join(ROOT, "data", "players", "赛甲.dat")
SAVE55 = os.path.join(ROOT, "data", "players", "赛乙.dat")
SAVE56 = os.path.join(ROOT, "data", "players", "逆甲.dat")
SAVE57 = os.path.join(ROOT, "data", "players", "逆乙.dat")
SAVE58 = os.path.join(ROOT, "data", "players", "禁甲.dat")
SAVE59 = os.path.join(ROOT, "data", "players", "禁乙.dat")
ALL_SAVES = [SAVE, SAVE2, SAVE3, SAVE4, SAVE5, SAVE6, SAVE7, SAVE8,
             SAVE9, SAVE10, SAVE11, SAVE12, SAVE13, SAVE14, SAVE15, SAVE16,
             SAVE17, SAVE18, SAVE19, SAVE20, SAVE21, SAVE22, SAVE23, SAVE24, SAVE25, SAVE26,
             SAVE27, SAVE28, SAVE29, SAVE30, SAVE31, SAVE32, SAVE33, SAVE34, SAVE35, SAVE36, SAVE37, SAVE38, SAVE39, SAVE40, SAVE41, SAVE42, SAVE43, SAVE44, SAVE45, SAVE46, SAVE47, SAVE48, SAVE49, SAVE50, SAVE51, SAVE52, SAVE53, SAVE54, SAVE55, SAVE56, SAVE57, SAVE58, SAVE59]
GUILD_FILE = os.path.join(ROOT, "data", "guilds.dat")
GUILDWARE_FILE = os.path.join(ROOT, "data", "guildware.dat")
BOSS_KILL_FILE = os.path.join(ROOT, "data", "boss_kills.dat")
DUNGEON_RANK_FILE = os.path.join(ROOT, "data", "dungeon_rank.dat")
DUNGEON_FIRST_FILE = os.path.join(ROOT, "data", "dungeon_first.dat")
SEASON_FILE = os.path.join(ROOT, "data", "season.dat")

ANSI = re.compile(r"\x1b\[[0-9;]*m|\x1b\][^\x07]*\x07")
EXP = re.compile(r"经验=(\d+)/(\d+)")


def run(commands):
    proc = subprocess.Popen(
        [BIN], cwd=ROOT,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True)
    out, _ = proc.communicate("\n".join(commands) + "\nexit\n", timeout=40)
    return ANSI.sub("", out)


def run_timed(initial, wait, rest):
    """按时间顺序喂命令，用于验证依赖服务器 tick 的行为（如自然恢复）。"""
    proc = subprocess.Popen(
        [BIN], cwd=ROOT,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True)
    for cmd in initial:
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()
    time.sleep(wait)
    for cmd in rest:
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()
    out, _ = proc.communicate("exit\n", timeout=30)
    return ANSI.sub("", out)


def main():
    if not os.path.exists(BIN):
        print("未找到 %s，请先 make" % BIN, file=sys.stderr)
        return 1
    for path in ALL_SAVES:
        if os.path.exists(path):
            os.remove(path)
    if os.path.exists(GUILD_FILE):
        os.remove(GUILD_FILE)
    if os.path.exists(GUILDWARE_FILE):
        os.remove(GUILDWARE_FILE)
    if os.path.exists(BOSS_KILL_FILE):
        os.remove(BOSS_KILL_FILE)
    if os.path.exists(DUNGEON_RANK_FILE):
        os.remove(DUNGEON_RANK_FILE)
    if os.path.exists(DUNGEON_FIRST_FILE):
        os.remove(DUNGEON_FIRST_FILE)
    if os.path.exists(SEASON_FILE):
        os.remove(SEASON_FILE)

    results = []

    def check(ok, message):
        results.append(bool(ok))
        print("  [%s] %s" % ("PASS" if ok else "FAIL", message))

    print("1) 启动与职业数据")
    out0 = run(["status", "boss"])
    check("职业: 3 种" in out0, "职业表加载")
    check(re.search(r"技能: \d+ 个", out0) is not None, "技能表加载")
    check("怪物模板: 已加载" in out0, "怪物模板加载")
    check("Boss: 42" in out0 and "沃玛教主" in out0, "Boss 刷新点")
    check("存活" in out0, "Boss 已刷出")

    print("2) 创建战士、装备、战斗、升级、技能、存档")
    out1 = run([
        "addbot " + NAME + " 战士",
        "info " + NAME,
        "give " + NAME + " 木剑",
        "equip " + NAME + " 0 1001",
        "info " + NAME,
        "testcombat " + NAME + " 鸡",
        "addexp " + NAME + " 1000",
        "info " + NAME,
        "skills " + NAME,
        "testskill " + NAME + " 基础剑术 鸡",
        "save",
    ])
    matches = EXP.findall(out1)
    m = matches[-1] if matches else None
    check("得到 木剑" in out1 or "获得 木剑" in out1, "按模板发物品")
    check("攻击 3-7" in out1, "1 级装备木剑后攻击 3-7")
    check("已击杀" in out1, "战斗击杀怪物")
    check("1 级 -> 3 级" in out1, "经验结算升级")
    check("等级=3" in out1, "升级后等级 3")
    check("攻杀剑术" in out1, "技能列表显示")
    check("施放 基础剑术：成功" in out1, "战士技能释放")
    check(os.path.exists(SAVE), "生成角色存档")

    exp_now = m[0] if m else ""

    print("3) 重启读档：等级/经验/装备恢复")
    out2 = run([
        "addbot " + NAME,
        "info " + NAME,
        "monsters",
    ])
    check("等级=3" in out2, "等级从存档恢复")
    check("攻击 7-13" in out2, "装备从存档恢复（3 级 + 木剑）")
    check(("经验=" + exp_now) in out2, "经验从存档恢复")
    check("当前怪物数量" in out2, "地图刷怪")

    print("4) 法师技能")
    out3 = run([
        "addbot 法测 法师",
        "restore 法测",
        "skills 法测",
        "testskill 法测 火球术 鸡",
    ])
    check("火球术" in out3, "法师技能列表")
    check("施放 火球术：成功" in out3, "法师技能释放")

    print("5) 死亡与复活")
    out4 = run([
        "addbot " + NAME,
        "kill " + NAME,
        "info " + NAME,
    ])
    check("已被杀死并复活" in out4, "死亡处理")
    check("HP=170/170" in out4, "复活满血（3 级战士 170 HP）")

    print("6) 商店、药品与传送")
    out5 = run([
        "addbot 商测 战士",
        "shop",
        "buy 商测 金创药 2",
        "hurt 商测 30",
        "use 商测 1001",
        "info 商测",
        "teleport 商测 2 200 200",
        "info 商测",
    ])
    check("商店在售" in out5, "商店列表")
    check("剩余 900" in out5, "购买扣金币")
    check("HP=50/50" in out5, "药品回血")
    check("地图=2" in out5, "传送到落霞岛")

    print("7) 自然恢复")
    out6 = run_timed(["addbot 回测 战士", "hurt 回测 40"], 4.2, ["info 回测"])
    m2 = re.search(r"HP=(\d+)/50", out6)
    check(m2 is not None and int(m2.group(1)) > 10, "3 秒后自然恢复（10 -> %s）" % (m2.group(1) if m2 else "?"))

    print("8) NPC：对话、商店、传送")
    out7 = run([
        "addbot NPC测 战士",
        "npcs",
        "talk NPC测 杂货商",
        "teleport NPC测 1 477 217",
        "talk NPC测 杂货商",
        "npcshop NPC测 杂货商",
        "teleport NPC测 1 480 227",
        "npcteleport NPC测 传送员",
        "info NPC测",
    ])
    check("当前 NPC: 50 个" in out7, "NPC 列表（50 个）")
    check("太远了" in out7, "距离校验")
    check("欢迎光临中州杂货铺" in out7, "NPC 对话")
    check("货架" in out7, "NPC 商店")
    check("通过 传送员 传送到地图 2" in out7, "NPC 传送")
    check("地图=2" in out7, "传送后地图 2")

    print("9) 物品堆叠")
    out8 = run([
        "addbot 堆测 战士",
        "buy 堆测 金创药 5",
        "bag 堆测",
        "hurt 堆测 30",
        "use 堆测 1001",
        "bag 堆测",
        "sell 堆测 1001",
        "bag 堆测",
    ])
    check("成功 5" in out8, "一次购买 5 个堆叠")
    check("金创药 x5" in out8, "背包显示 x5")
    check("金创药 x4" in out8, "使用后 x4")
    check("金创药 x3" in out8, "卖出后 x3")

    print("10) 技能书学习与持久化")
    out9 = run([
        "addbot 书测 战士",
        "addexp 书测 100000000",
        "give 书测 烈火剑法书",
        "use 书测 1001",
        "skills 书测",
        "testskill 书测 烈火剑法 鸡",
        "save",
    ])
    check("获得 烈火剑法书" in out9, "发放技能书")
    check("[已学] 烈火剑法" in out9, "使用技能书学会")
    check("施放 烈火剑法：成功" in out9, "学会后可释放")

    out10 = run(["addbot 书测", "skills 书测"])
    check("[已学] 烈火剑法" in out10, "已学技能随存档恢复")

    print("11) 任务：杀怪、收集、交付与持久化")
    quest_cmds = [
        "addbot 任测 战士",
        "teleport 任测 1 477 217",
        "quests 任测",
        "accept 任测 除害",
    ]
    quest_cmds += ["testcombat 任测 鸡"] * 10
    quest_cmds += [
        "quests 任测",
        "submit 任测 除害",
        "buy 任测 金创药 5",
        "accept 任测 收集药材",
        "submit 任测 收集药材",
        "quests 任测",
        "save",
    ]
    out11 = run(quest_cmds)
    check("接受任务 [除害]" in out11, "接取杀怪任务")
    check("进度 10/10" in out11, "杀怪进度累计")
    check("提交任务 [除害]：经验 +500，金币 +100" in out11, "交付杀怪任务与奖励")
    check("提交任务 [收集药材]：经验 +300，金币 +50" in out11, "交付收集任务与奖励")
    check(out11.count("[已完成]") >= 2, "两个任务均已完成")

    out12 = run(["addbot 任测", "quests 任测"])
    check(out12.count("[已完成]") >= 2, "任务进度随存档恢复")

    print("12) 装备强化 / 拆分 / 批量卖出")
    out13 = run([
        "addbot 强测 战士",
        "give 强测 木剑",
        "equip 强测 0 1001",
        "upgrade 强测 0 1001",
        "equipinfo 强测",
        "info 强测",
        "buy 强测 金创药 5",
        "split 强测 1002 3",
        "bag 强测",
        "sellall 强测 金创药",
    ])
    check("强化到 +1" in out13, "装备强化")
    check("木剑 +1" in out13, "装备列表显示强化")
    check("攻击 4-8" in out13, "强化后攻击提升")
    check("金创药 x2" in out13 and "金创药 x3" in out13, "堆叠拆分")
    check("批量卖出 金创药 x5" in out13, "批量卖出")

    print("13) 组队：邀请、频道、经验分享")
    out14 = run([
        "addbot 队甲 战士",
        "addbot 队乙 战士",
        "partyinvite 队甲 队乙",
        "party 队甲",
        "psay 队甲 集合",
        "testcombat 队甲 鸡",
        "info 队乙",
    ])
    check("队伍（2 人）" in out14, "组队邀请")
    check("【队伍】队甲: 集合" in out14, "队伍频道")
    check("经验=5/110" in out14, "组队经验分享")

    print("14) 行会：创建、加入、频道、移交")
    out15 = run([
        "addbot 会甲 战士",
        "guild create 会甲 测试会",
        "guild info 测试会",
        "addbot 会乙 战士",
        "guild join 会乙 测试会",
        "gsay 会甲 开工",
        "guild leave 会甲",
        "guild info 测试会",
        "guild leave 会乙",
    ])
    check("创建了行会 [测试会]" in out15, "创建行会")
    check("【行会】会甲: 开工" in out15, "行会频道")
    check("会长 会乙" in out15, "会长移交")

    print("15) 第三张地图与 Boss 数据")
    out16 = run([
        "addbot 图测 战士",
        "teleport 图测 1 482 232",
        "npcteleport 图测 矿洞向导",
        "info 图测",
    ])
    check("通过 矿洞向导 传送到地图 3" in out16, "NPC 传送矿洞")
    check("地图=3" in out16, "到达矿洞")

    for path in ALL_SAVES:
        if os.path.exists(path):
            os.remove(path)
    if os.path.exists(GUILD_FILE):
        os.remove(GUILD_FILE)
    if os.path.exists(GUILDWARE_FILE):
        os.remove(GUILDWARE_FILE)
    if os.path.exists(BOSS_KILL_FILE):
        os.remove(BOSS_KILL_FILE)
    if os.path.exists(DUNGEON_RANK_FILE):
        os.remove(DUNGEON_RANK_FILE)
    if os.path.exists(DUNGEON_FIRST_FILE):
        os.remove(DUNGEON_FIRST_FILE)
    if os.path.exists(SEASON_FILE):
        os.remove(SEASON_FILE)

    print("16) 任务链与日常任务")
    chain = [
        "addbot 链测 战士",
        "teleport 链测 1 477 217",
        "addexp 链测 1000",
        "accept 链测 剿灭多钩猫",
        "accept 链测 除害",
    ]
    chain += ["testcombat 链测 鸡"] * 10
    chain += ["submit 链测 除害", "accept 链测 剿灭多钩猫"]
    chain += ["testcombat 链测 多钩猫"] * 5
    chain += ["submit 链测 剿灭多钩猫", "accept 链测 日常除害"]
    chain += ["testcombat 链测 鸡"] * 5
    chain += ["submit 链测 日常除害", "accept 链测 日常除害", "quests 链测"]
    out17 = run(chain)
    check("接受失败" in out17, "前置任务拦截")
    check("接受任务 [剿灭多钩猫]" in out17, "完成前置后可接任务链")
    check("提交任务 [剿灭多钩猫]：经验 +800" in out17, "任务链交付")
    check("提交任务 [日常除害]：经验 +200" in out17, "日常任务交付")
    check("日常除害（日常）" in out17 and out17.count("接受任务 [日常除害]") >= 2, "日常任务可重复接取")

    print("17) NPC 对话树")
    out18 = run([
        "addbot 谈测 战士",
        "teleport 谈测 1 477 217",
        "talk 谈测 杂货商",
        "choose 谈测 杂货商 1",
        "choose 谈测 杂货商 2",
        "choose 谈测 杂货商 3",
        "teleport 谈测 1 480 227",
        "choose 谈测 传送员 1",
        "info 谈测",
    ])
    check("1. 看看货架" in out18, "对话选项列表")
    check("商店在售" in out18, "对话打开商店")
    check("再见" in out18, "关闭对话")
    check("通过对话传送到地图 2" in out18, "对话传送")
    check("地图=2" in out18, "对话传送到达")

    print("18) 行会等级/权限、双倍经验与公告")
    out19 = run([
        "addbot 会测 战士",
        "addgold 会测 5000",
        "guild create 会测 活动会",
        "addbot 会测2 战士",
        "guild join 会测2 活动会",
        "guild appoint 会测 会测2",
        "guild donate 会测 5000",
        "guild info 活动会",
        "guild kick 会测 会测2",
        "event doubleexp 60",
        "event status",
        "testcombat 会测 鸡",
        "announce 测试公告",
    ])
    check("副会长 会测2" in out19, "任命副会长")
    check("行会 [活动会] 升到 2 级" in out19, "行会资金升级")
    check("等级 2  资金 5000" in out19, "行会信息显示等级与资金")
    check("踢出了行会" in out19, "会长踢人")
    check("双倍经验：进行中" in out19, "双倍经验活动")
    check("角色经验 22" in out19, "双倍经验+行会加成（10→22）")
    check("【公告】测试公告" in out19, "全服公告")

    print("19) 行会仓库与等级增益")
    out20 = run([
        "addbot 仓测 战士",
        "addgold 仓测 5000",
        "guild create 仓测 仓库会",
        "give 仓测 木剑",
        "guild store 仓测 1001",
        "guild ware 仓测",
        "guild take 仓测 0",
        "guild donate 仓测 5000",
        "guild info 仓库会",
        "testcombat 仓测 鸡",
        "guild store 仓测 1001",
        "save",
    ])
    check("存入行会仓库（1/50）" in out20, "存入行会仓库")
    check("木剑" in out20, "仓库列表显示物品")
    check("从行会仓库取出 木剑" in out20, "从仓库取出")
    check("成员 1/30" in out20 and "经验加成 +10%" in out20, "等级带来人数/经验增益")
    check("角色经验 11" in out20, "行会经验加成（10→11）")

    out21 = run(["guild ware 仓测"])
    check("木剑" in out21, "仓库内容随文件持久化")

    print("20) 多掉落、仓库权限、活动排期")
    out22 = run([
        "addbot 权测 战士",
        "addgold 权测 5000",
        "guild create 权测 权限会",
        "addbot 权测2 战士",
        "guild join 权测2 权限会",
        "give 权测2 木剑",
        "guild store 权测2 1001",
        "guild ware 权测",
        "guild take 权测2 0",
        "guild take 权测 0",
        "drops 鸡",
        "event list",
        "event trigger 0",
        "event status",
        "testcombat 权测 鸡",
    ])
    check("存入行会仓库（1/50）" in out22, "成员可存入仓库")
    check("只有会长/副会长可以从行会仓库取出物品" in out22, "成员取出被拒绝")
    check("从行会仓库取出 木剑" in out22, "会长可取回")
    check("金创药  100%" in out22, "多掉落表")
    check("活动排期: 2 条" in out22, "活动排期加载")
    check("双倍经验：进行中" in out22, "排期活动触发")
    check("角色经验 20" in out22, "排期双倍经验生效")

    print("21) 沃玛寺庙副本与挑战任务")
    out23 = run([
        "addbot 寺测 战士",
        "teleport 寺测 1 486 217",
        "npcteleport 寺测 寺庙守卫",
        "info 寺测",
        "boss",
        "quests 寺测",
    ])
    check("通过 寺庙守卫 传送到地图 4" in out23, "寺庙守卫传送")
    check("地图=4" in out23, "到达沃玛寺庙")
    check("沃玛教主  地图 4 (220,200)" in out23, "寺庙 Boss 刷新点")
    check("[未达前置] 挑战沃玛教主" in out23, "挑战任务前置校验")

    print("22) 新增地图与内容扩展")
    out24 = run([
        "addbot 新图测 战士",
        "teleport 新图测 1 490 217",
        "npcteleport 新图测 将军坟守卫",
        "teleport 新图测 5 258 258",
        "npcteleport 新图测 尸王殿入口",
        "teleport 新图测 7 256 256",
        "boss",
        "drops 尸王",
    ])
    check("通过 将军坟守卫 传送到地图 5" in out24, "将军坟入口")
    check("通过 尸王殿入口 传送到地图 6" in out24, "尸王殿入口")
    check("传送到 蛇魔谷" in out24, "蛇魔谷传送")
    check("尸王  地图 6 (280,240)" in out24, "尸王刷新点")
    check("蛇魔  地图 7 (280,240)" in out24, "蛇魔刷新点")
    check("铁剑" in out24, "尸王多掉落")

    print("23) 套装属性与 Boss 首杀")
    out25 = run([
        "addbot 套测 战士",
        "addexp 套测 5000000",
        "give 套测 重盔甲(男)",
        "give 套测 骷髅头盔",
        "give 套测 裁决之杖",
        "equip 套测 1 1001",
        "equip 套测 2 1002",
        "equip 套测 0 1003",
        "sets 套测",
        "info 套测",
        "bosskill 0 套测",
        "firstkills",
    ])
    check("已激活] 战士套装" in out25, "套装激活")
    check("攻击 72-113" in out25, "套装属性生效（攻击 72-113）")
    check("首杀 沃玛教主（地图 3）" in out25, "Boss 首杀公告")
    check("首杀记录: 1 条" in out25, "首杀记录")

    out26 = run(["addbot 套测", "bosskill 0 套测", "firstkills"])
    check("首杀记录: 1 条" in out26, "首杀记录持久化")
    check("首杀 沃玛教主" not in out26, "重复击杀不再公告")

    print("24) 装备附魔与洗炼")
    out27 = run([
        "addbot 附测 战士",
        "addgold 附测 10000",
        "give 附测 木剑",
        "equip 附测 0 1001",
        "enchant 附测 0 1001 1",
        "equipinfo 附测",
        "info 附测",
        "refine 附测 0 1001",
        "equipinfo 附测",
    ])
    check("附魔 火+1" in out27, "武器附魔火")
    check("元素1+1" in out27, "装备信息显示元素")
    check("攻击 4-8" in out27, "火元素加攻击（3-7 → 4-8）")
    check("洗炼为" in out27, "洗炼元素")

    print("25) 世界 Boss 排期")
    out28 = run([
        "addbot 世测 战士",
        "event list",
        "event trigger 1",
        "boss",
        "bosskill 42 世测",   # 42 个静态 Boss 后，世界 Boss 动态索引为 42
        "boss",
        "firstkills",
    ])
    check("活动排期: 2 条" in out28, "双倍经验 + 世界Boss 排期")
    check("世界 Boss【沃玛教主】降临 地图 4 (280,240)" in out28, "世界 Boss 降临公告")
    check("沃玛教主  地图 4 (280,240)  存活" in out28, "世界 Boss 登记")
    check("已击杀（世界Boss，不重刷）" in out28, "世界 Boss 不自动重刷")
    check("首杀 沃玛教主（地图 4）" in out28, "世界 Boss 首杀")

    print("26) 组队副本：沃玛神殿")
    out29 = run([
        "addbot 副甲 战士",
        "addbot 副乙 战士",
        "partyinvite 副甲 副乙",
        "teleport 副甲 1 496 217",
        "npcteleport 副甲 副本使者",
        "info 副甲",
        "info 副乙",
        "dungeon",
        "bosskill 4 副甲",
        "titles 副甲",
        "dungeon",
    ])
    check("队伍进入【沃玛神殿】1 号实例（普通，地图 8，2 人）" in out29, "队伍整队进入副本")
    check(out29.count("地图=8") >= 2, "两名队员都在副本")
    check("1（普通）：队伍 副甲" in out29, "副本锁占用")
    check("通关沃玛神殿" in out29, "击杀 Boss 通关")
    check("首通沃玛神殿" in out29 and "沃玛首通者" in out29, "首通称号")
    check("1（普通）：空闲" in out29, "通关后副本释放")

    print("27) 副本排行榜与每日首通")
    out30 = run([
        "addbot 榜甲 战士",
        "addbot 榜乙 战士",
        "partyinvite 榜甲 榜乙",
        "teleport 榜甲 1 496 217",
        "npcteleport 榜甲 副本使者",
        "bosskill 4 榜甲",
        "dungeonrank",
    ])
    check("每日首通奖励" in out30, "每日首通奖励")
    check("沃玛神殿：" in out30 and "榜甲  用时" in out30, "分副本排行榜")
    check("榜甲  用时" in out30, "排行榜显示队伍与用时")

    print("28) 副本多实例并行")
    out31 = run([
        "addbot 并甲 战士",
        "addbot 并乙 战士",
        "addbot 并丙 战士",
        "addbot 并丁 战士",
        "partyinvite 并甲 并乙",
        "partyinvite 并丙 并丁",
        "teleport 并甲 1 496 217",
        "teleport 并丙 1 496 217",
        "npcteleport 并甲 副本使者",
        "npcteleport 并丙 副本使者",
        "dungeon",
        "bosskill 4 并甲",
        "bosskill 5 并丙",
        "dungeon",
    ])
    check("1 号实例（普通，地图 8" in out31 and "2 号实例（困难，地图 9" in out31, "两支队伍进入不同实例")
    check(out31.count("1（普通）：队伍") >= 1 and out31.count("2（困难）：队伍") >= 1, "副本槽位并行占用")
    check(out31.count("通关沃玛神殿") >= 2, "两个实例分别通关")
    check("1（普通）：空闲" in out31 and "2（困难）：空闲" in out31, "通关后槽位释放")

    print("29) 副本怪物独立重置")
    out32 = run([
        "addbot 重甲 战士",
        "addbot 重乙 战士",
        "partyinvite 重甲 重乙",
        "teleport 重甲 1 496 217",
        "npcteleport 重甲 副本使者",
        "mapmonsters 8",
        "bosskill 4 重甲",
        "teleport 重甲 1 496 217",
        "npcteleport 重甲 副本使者",
        "mapmonsters 8",
        "boss",
    ])
    check(out32.count("1 号实例（普通，地图 8") >= 2, "通关后再次进入同一实例")
    check("地图 8 当前怪物: 0" not in out32, "进入时重新刷怪")
    check("沃玛教主  地图 8 (280,240)  存活" in out32, "再次进入重置 Boss")

    print("31) 成就与称号")
    out33 = run([
        "addbot 成甲 战士",
        "addbot 成乙 战士",
        "partyinvite 成甲 成乙",
        "addexp 成甲 8000000",
        "title 成甲 1",
        "info 成甲",
        "teleport 成甲 1 496 217",
        "npcteleport 成甲 副本使者",
        "bosskill 4 成甲",
        "achievements 成甲",
        "achievements 成乙",
    ])
    check("已完成] 1. 初出茅庐" in out33, "等级成就")
    check("已完成] 2. 一方强者" in out33, "30 级成就")
    check("称号：【初出茅庐】" in out33, "称号显示")
    check("HP=1930/1930" in out33, "称号 HP 加成")
    check("70 点" in out33, "成就点数")
    check("已完成] 3. 沃玛终结者" in out33, "Boss 首杀成就")
    check("已完成] 4. 神殿征服者" in out33, "副本通关成就")
    check("成乙 的成就（1/15" in out33, "队员同样获得成就")

    out34 = run(["addbot 成甲", "achievements 成甲"])
    check("成甲 的成就（4/15" in out34, "成就持久化")

    print("33) 成就点数商店与称号")
    out35 = run([
        "addbot 点甲 战士",
        "addexp 点甲 8000000",
        "pointshop 点甲",
        "pointbuy 点甲 101",
        "title 点甲 101",
        "info 点甲",
        "titles 点甲",
    ])
    check("消耗 20 点兑换称号【新兵】" in out35, "点数兑换称号")
    check("称号：【新兵】" in out35, "佩戴兑换称号")
    check("HP=1915/1915" in out35, "称号加成生效")
    check("可用 10 点" in out35, "点数扣减")

    out36 = run(["addbot 点甲", "titles 点甲", "info 点甲"])
    check("当前【新兵】" in out36 and "HP=1915/1915" in out36, "称号与点数持久化")

    print("34) 尸王殿 / 蛇魔谷副本")
    out37 = run([
        "addbot 尸甲 战士",
        "addbot 尸乙 战士",
        "partyinvite 尸甲 尸乙",
        "teleport 尸甲 1 500 217",
        "npcteleport 尸甲 尸王殿守卫",
        "bosskill 7 尸甲",
        "titles 尸甲",
        "addbot 蛇甲 战士",
        "addbot 蛇乙 战士",
        "partyinvite 蛇甲 蛇乙",
        "teleport 蛇甲 1 503 217",
        "npcteleport 蛇甲 蛇谷守卫",
        "bosskill 9 蛇甲",
        "titles 蛇甲",
        "dungeonrank",
        "dungeon",
    ])
    check("进入【尸王殿】1 号实例（普通，地图 11" in out37, "尸王殿副本进入")
    check("进入【蛇魔谷】1 号实例（普通，地图 13" in out37, "蛇魔谷副本进入")
    check("通关尸王殿" in out37 and "通关蛇魔谷" in out37, "两个副本通关")
    check("尸王首通者" in out37 and "蛇魔首通者" in out37, "尸王/蛇魔首通称号")
    check("尸王殿：" in out37 and "蛇魔谷：" in out37, "分副本排行显示")
    check("尸王殿（3 个实例）" in out37 and "蛇魔谷（3 个实例）" in out37 and out37.count("1（普通）：空闲") >= 2, "副本实例释放")

    print("35) 矿洞 / 将军坟副本")
    out38 = run([
        "addbot 矿甲 战士",
        "addbot 矿乙 战士",
        "partyinvite 矿甲 矿乙",
        "teleport 矿甲 1 506 217",
        "npcteleport 矿甲 矿洞守卫",
        "bosskill 11 矿甲",
        "titles 矿甲",
        "addbot 坟甲 战士",
        "addbot 坟乙 战士",
        "partyinvite 坟甲 坟乙",
        "teleport 坟甲 1 509 217",
        "npcteleport 坟甲 将军坟副本守卫",
        "bosskill 13 坟甲",
        "titles 坟甲",
        "dungeon",
    ])
    check("进入【矿洞】1 号实例（普通，地图 15" in out38, "矿洞副本进入")
    check("进入【将军坟】1 号实例（普通，地图 17" in out38, "将军坟副本进入")
    check("通关矿洞" in out38 and "通关将军坟" in out38, "两个副本通关")
    check("矿洞首通者" in out38 and "将军坟首通者" in out38, "矿洞/将军坟首通称号")
    check("矿洞（3 个实例）" in out38 and "将军坟（3 个实例）" in out38 and "3（地狱）：空闲（地图 24）" in out38 and "3（地狱）：空闲（地图 25）" in out38, "副本列表与地狱档")

    print("36) 副本专属套装")
    out39 = run([
        "addbot 套甲 战士",
        "addexp 套甲 5000000",
        "give 套甲 沃玛头盔",
        "give 套甲 沃玛项链",
        "give 套甲 沃玛戒指",
        "equip 套甲 2 1001",
        "equip 套甲 3 1002",
        "equip 套甲 4 1003",
        "sets 套甲",
        "info 套甲",
    ])
    check("已激活] 沃玛套装" in out39, "沃玛套装激活")
    check("攻击 62-91" in out39, "套装属性（DC+5）")

    print("37) 祖玛寺庙副本（普通/困难/地狱）")
    out40 = run([
        "addbot 祖甲 战士",
        "addbot 祖乙 战士",
        "partyinvite 祖甲 祖乙",
        "teleport 祖甲 1 512 217",
        "npcteleport 祖甲 祖玛使者",
        "bosskill 15 祖甲",
        "dungeon",
    ])
    check("进入【祖玛寺庙】1 号实例（普通，地图 19" in out40, "祖玛副本进入")
    check("祖玛寺庙（3 个实例）" in out40 and "1（普通）：空闲" in out40 and "3（地狱）：空闲（地图 21）" in out40, "祖玛三档难度")
    check("通关祖玛寺庙" in out40, "祖玛通关")

    print("38) 套装图鉴收集")
    out41 = run([
        "addbot 图甲 战士",
        "give 图甲 沃玛头盔",
        "give 图甲 沃玛项链",
        "give 图甲 沃玛戒指",
        "codex 图甲",
        "save",
    ])
    check("物品 3 件" in out41, "图鉴记录物品")
    check("沃玛套装  3/3" in out41 and ("已奖励" in out41 or "已集齐" in out41), "套装图鉴集齐")

    out42 = run(["addbot 图甲", "codex 图甲"])
    check("物品 3 件" in out42 and "沃玛套装  3/3" in out42, "图鉴持久化")

    print("39) 图鉴集齐奖励")
    out43 = run([
        "addbot 集甲 战士",
        "give 集甲 沃玛头盔",
        "give 集甲 沃玛项链",
        "give 集甲 沃玛戒指",
        "codex 集甲",
        "save",
    ])
    check("集齐套装【沃玛套装】" in out43, "集齐套装公告")
    check("已奖励] 沃玛套装" in out43, "图鉴奖励标记")

    out44 = run(["addbot 集甲", "codex 集甲"])
    check("已奖励] 沃玛套装  3/3" in out44, "图鉴奖励持久化")

    print("40) 图鉴里程碑称号")
    out45 = run([
        "addbot 鉴甲 战士",
        "give 鉴甲 沃玛头盔",
        "give 鉴甲 沃玛项链",
        "give 鉴甲 沃玛戒指",
        "give 鉴甲 尸王头盔",
        "give 鉴甲 尸王项链",
        "give 鉴甲 尸王戒指",
        "give 鉴甲 蛇魔头盔",
        "give 鉴甲 蛇魔项链",
        "give 鉴甲 蛇魔戒指",
        "titles 鉴甲",
    ])
    check("集齐 3 套图鉴" in out45, "图鉴里程碑公告")
    check("收藏家" in out45, "图鉴称号【收藏家】")

    print("41) 排行赛季结算")
    out46 = run([
        "addbot 赛甲 战士",
        "addbot 赛乙 战士",
        "partyinvite 赛甲 赛乙",
        "teleport 赛甲 1 496 217",
        "npcteleport 赛甲 副本使者",
        "bosskill 4 赛甲",
        "season status",
        "season settle",
        "titles 赛甲",
        "season status",
        "dungeonrank",
    ])
    check("第 1 赛季" in out46, "赛季结算公告")
    check("赛季冠军" in out46, "赛季冠军称号")
    check("当前第 2 赛季" in out46, "赛季号递增")
    check("暂无记录" in out46, "结算后清空排行")

    print("42) 逆魔古刹 / 禁地副本")
    out47 = run([
        "addbot 逆甲 战士",
        "addbot 逆乙 战士",
        "partyinvite 逆甲 逆乙",
        "teleport 逆甲 1 502 217",
        "npcteleport 逆甲 逆魔使者",
        "bosskill 22 逆甲",
        "addbot 禁甲 战士",
        "addbot 禁乙 战士",
        "partyinvite 禁甲 禁乙",
        "teleport 禁甲 1 505 217",
        "npcteleport 禁甲 禁地使者",
        "bosskill 25 禁甲",
        "dungeon",
    ])
    check("进入【逆魔古刹】1 号实例（普通，地图 26" in out47, "逆魔古刹副本进入")
    check("进入【禁地】1 号实例（普通，地图 29" in out47, "禁地副本进入")
    check("通关逆魔古刹" in out47 and "通关禁地" in out47, "两个副本通关")
    check("逆魔古刹（3 个实例）" in out47 and "禁地（3 个实例）" in out47, "副本列表")

    ok = all(results)
    if ok:
        print("\n全部通过 ✅ (%d 项)" % len(results))
        return 0
    print("\n存在失败项 ❌")
    for name, out in (("启动", out0), ("首轮", out1), ("读档", out2), ("法师", out3), ("死亡", out4)):
        print("\n--- %s输出 ---\n%s" % (name, out))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
