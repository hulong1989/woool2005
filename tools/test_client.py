#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GameServer 重建骨架的测试客户端。

用法:
    python3 tools/test_client.py --name 张三
    python3 tools/test_client.py --name 张三 --say 你好 --walk 2
    python3 tools/test_client.py --raw "bag" --raw "whoami"

协议:
    帧 = 4 字节小端长度 + 载荷
    文本指令（未编码）: "enter <名字>" / "say <内容>" / "walk <0-7>" / "whoami"
    服务器下行: '#' + 16 字节编码头 + 编码负载 + '!'
"""
import argparse
import socket
import struct
import sys
import time

SM_NAMES = {
    0x0101: "SM_SETMAP",
    0x0102: "SM_SETPLAYER",
    0x0103: "SM_SETPLAYERNAME",
    0x0104: "SM_SETMAPNAME",
    0x0105: "SM_READY",
    0x0106: "SM_APPEAR",
    0x0107: "SM_DISAPPEAR",
    0x0108: "SM_WALK",
    0x0109: "SM_RUN",
    0x010A: "SM_TURN",
    0x010B: "SM_ATTACK",
    0x010C: "SM_BEATTACK",
    0x010D: "SM_CHAT",
    0x010E: "SM_BACK",
    0x010F: "SM_UPDATEPROP",
    0x0110: "SM_DOWNITEMAPPEAR",
    0x0111: "SM_DOWNITEMDISAPPEAR",
    0x0112: "SM_ADDITEM",
    0x0113: "SM_EQUIPRESULT",
    0x0114: "SM_UNEQUIPRESULT",
    0x0115: "SM_ACTIONRESULT",
    0x0116: "SM_DEATH",
    0x0117: "SM_SPELL",
    0x0118: "SM_LEVELUP",
    0x02C4: "SM_SETFIGHTMAP",
}


def frame(payload: bytes) -> bytes:
    return struct.pack("<I", len(payload)) + payload


def decode(buf: bytes) -> bytes:
    return bytes(b ^ ((0x5A + i * 0x1D) & 0xFF) for i, b in enumerate(buf))


def describe(pkt: bytes) -> str:
    if not pkt.startswith(b"#"):
        try:
            return "TEXT  " + pkt.decode("utf-8", "replace")
        except Exception:
            return "BINARY " + pkt.hex()
    if len(pkt) < 18:
        return "SHORT " + pkt.hex()

    hdr = decode(pkt[1:17])
    flag, cmd = struct.unpack_from("<IH", hdr, 0)
    p0, p1, p2 = struct.unpack_from("<HHH", hdr, 6)
    name = SM_NAMES.get(cmd, "UNKNOWN")
    payload = decode(pkt[17:-1]) if len(pkt) > 18 else b""
    text = ""
    if payload:
        try:
            printable = payload.decode("utf-8")
            if printable.isprintable():
                text = "  text=%r" % printable
        except UnicodeDecodeError:
            text = "  payload=" + payload[:16].hex()
    return "%-18s cmd=0x%04x flag=0x%08x p=(%d,%d,%d) len=%d%s" % (
        name, cmd, flag, p0, p1, p2, len(payload), text)


def recv_frames(sock: socket.socket, deadline: float):
    data = b""
    sock.settimeout(0.2)
    while time.time() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data += chunk

    out = []
    i = 0
    while i + 4 <= len(data):
        n = struct.unpack_from("<I", data, i)[0]
        pkt = data[i + 4:i + 4 + n]
        if len(pkt) < n:
            break
        out.append(pkt)
        i += 4 + n
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="GameServer 重建骨架测试客户端")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7100)
    ap.add_argument("--name", default="测试角色", help="进入游戏的角色名")
    ap.add_argument("--class", dest="cls", default="0", choices=["0", "1", "2", "战士", "法师", "道士"],
                    help="职业：0/战士 1/法师 2/道士（仅新角色生效）")
    ap.add_argument("--no-enter", action="store_true", help="不自动发送 enter")
    ap.add_argument("--say", help="进入后发送 say <内容>")
    ap.add_argument("--walk", type=int, help="进入后发送 walk <方向 0-7>")
    ap.add_argument("--cast", help="进入后发送 cast <技能名>")
    ap.add_argument("--raw", action="append", default=[], help="发送任意文本指令，可重复")
    ap.add_argument("--seconds", type=float, default=1.5, help="收包等待时间")
    args = ap.parse_args()

    try:
        sock = socket.create_connection((args.host, args.port), timeout=3)
    except OSError as exc:
        print("连接失败: %s" % exc, file=sys.stderr)
        print("请先启动服务器: ./build/GameServer", file=sys.stderr)
        return 1

    try:
        if not args.no_enter:
            sock.sendall(frame(("enter " + args.name + " " + args.cls).encode("utf-8")))
            time.sleep(0.25)
        if args.cast:
            sock.sendall(frame(("cast " + args.cast).encode("utf-8")))
            time.sleep(0.15)
        if args.say:
            sock.sendall(frame(("say " + args.say).encode("utf-8")))
            time.sleep(0.15)
        if args.walk is not None:
            sock.sendall(frame(("walk %d" % args.walk).encode("utf-8")))
            time.sleep(0.15)
        for cmd in args.raw:
            sock.sendall(frame(cmd.encode("utf-8")))
            time.sleep(0.15)

        packets = recv_frames(sock, time.time() + args.seconds)
    finally:
        sock.close()

    print("收到 %d 个包:" % len(packets))
    for pkt in packets:
        print("  " + describe(pkt))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
