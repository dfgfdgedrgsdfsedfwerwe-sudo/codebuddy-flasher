#!/usr/bin/env python3
"""Quick verification test - check if system works"""
import socket
import json
import sys

def check_daemon():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        sock.connect(("127.0.0.1", 47100))
        sock.close()
        return True
    except:
        return False

def send_test_decision():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(15.0)
        sock.connect(("127.0.0.1", 47100))

        request = json.dumps({
            "type": "decision",
            "title": "Quick test:",
            "options": ["OK", "Cancel"]
        }) + "\n"

        sock.sendall(request.encode("utf-8"))

        response_data = b""
        while b"\n" not in response_data:
            response_data += sock.recv(1024)

        sock.close()
        return json.loads(response_data.decode("utf-8"))
    except Exception as e:
        return {"ok": False, "error": str(e)}

def main():
    print("=" * 60)
    print("  CodeBuddy AI BOX Quick Test")
    print("=" * 60)

    print("\n[1/2] Checking daemon...")
    if not check_daemon():
        print("\n[ERROR] Daemon not running!")
        print("\nPlease start:")
        print("  1. Run CodeBuddyBridge.exe")
        print("  2. Click 'Start Service'")
        return 1

    print("[OK] Daemon is running")

    print("\n[2/2] Sending test decision...")
    print("\nChoose input method:")
    print("  - ATK BOX: Touch screen + confirm")
    print("  - PC GUI: Click button in decision panel")
    print("\nWaiting (15s timeout)...\n")

    result = send_test_decision()

    if result.get("ok"):
        chosen = result.get("chosen")
        source = result.get("source", "unknown")
        options = ["OK", "Cancel"]

        print("\n" + "=" * 60)
        print("  [SUCCESS] Test passed!")
        print("=" * 60)
        print(f"\nChoice: [{chosen}] {options[chosen]}")
        print(f"Source: {source.upper()}")

        if source == "box":
            print("\n[OK] BOX touch working")
        elif source == "pc":
            print("\n[OK] PC input working (GUI)")

        print("\nSystem ready!")
        return 0
    else:
        print("\n" + "=" * 60)
        print("  [FAILED] Test failed")
        print("=" * 60)
        print(f"\nError: {result.get('error')}")
        print("\nCheck:")
        print("  - Dongle connected to COM6")
        print("  - ATK BOX powered on")
        print("  - GUI service started")
        return 1

if __name__ == "__main__":
    sys.exit(main())
