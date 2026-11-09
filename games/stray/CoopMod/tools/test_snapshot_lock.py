"""Hold a generated snapshot file exclusively to test transient Windows sharing failures."""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('snapshot',type=Path)
    p.add_argument('--seconds',type=float,default=3.5)
    a=p.parse_args()
    if a.snapshot.name!='remote.tsv': p.error('Only generated remote.tsv snapshots may be locked')
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    create=kernel.CreateFileW
    create.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    create.restype=wintypes.HANDLE
    close=kernel.CloseHandle
    close.argtypes=[wintypes.HANDLE]
    close.restype=wintypes.BOOL
    handle=create(str(a.snapshot.resolve()),0x80000000,0,None,3,128,None)
    if handle==ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
    try:
        print('Snapshot locked for sharing-failure regression test',flush=True)
        time.sleep(a.seconds)
    finally:
        close(handle)
    print('Snapshot unlocked',flush=True)

if __name__=='__main__': main()
