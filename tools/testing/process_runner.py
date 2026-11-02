"""Bounded subprocesses and cross-run resource locks; no test semantics here."""
from __future__ import annotations

import contextlib
import ctypes
import os
from pathlib import Path
import signal
import subprocess
import time


class ProcessTimeout(RuntimeError):
    pass


if os.name == "nt":
    from ctypes import wintypes

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [(n, ctypes.c_ulonglong) for n in
                    ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                     "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class BASIC_LIMIT(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                    ("PerJobUserTimeLimit", ctypes.c_longlong), ("LimitFlags", wintypes.DWORD),
                    ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t),
                    ("ActiveProcessLimit", wintypes.DWORD), ("Affinity", ctypes.c_size_t),
                    ("PriorityClass", wintypes.DWORD), ("SchedulingClass", wintypes.DWORD)]

    class EXTENDED_LIMIT(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", BASIC_LIMIT), ("IoInfo", IO_COUNTERS),
                    ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

    class ACCOUNTING(ctypes.Structure):
        _fields_ = [(n, ctypes.c_longlong) for n in
                    ("TotalUserTime", "TotalKernelTime", "ThisPeriodTotalUserTime", "ThisPeriodTotalKernelTime")]
        _fields_ += [(n, wintypes.DWORD) for n in
                     ("TotalPageFaultCount", "TotalProcesses", "ActiveProcesses", "TotalTerminatedProcesses")]

    class WindowsJob:
        """Assign a suspended process before allowing it to create descendants."""

        def __init__(self):
            self.api = ctypes.WinDLL("kernel32", use_last_error=True)
            self.api.CreateJobObjectW.restype = wintypes.HANDLE
            self.api.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
            self.api.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
            self.api.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
            self.api.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
            self.api.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                                         wintypes.DWORD, ctypes.c_void_p]
            self.api.CloseHandle.argtypes = [wintypes.HANDLE]
            self.api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
            self.api.OpenProcess.restype = wintypes.HANDLE
            self.api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            self.handle = self.api.CreateJobObjectW(None, None)
            if not self.handle:
                raise ctypes.WinError(ctypes.get_last_error())
            info = EXTENDED_LIMIT()
            info.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
            if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(info), ctypes.sizeof(info)):
                self.close()
                raise ctypes.WinError(ctypes.get_last_error())

        def attach_and_resume(self, proc):
            if not self.api.AssignProcessToJobObject(self.handle, int(proc._handle)):
                proc.kill()
                proc.wait()
                raise ctypes.WinError(ctypes.get_last_error())
            resume = ctypes.WinDLL("ntdll").NtResumeProcess
            resume.argtypes = [wintypes.HANDLE]
            resume.restype = ctypes.c_long
            if resume(int(proc._handle)) != 0:
                raise RuntimeError("Cannot resume subprocess in its job")

        def cleanup(self):
            # ActiveProcesses may become zero before process handle teardown completes.
            # Retain waitable handles for every live job member before terminating it.
            members = []
            size = 4096
            while True:
                listing = ctypes.create_string_buffer(size)
                if self.api.QueryInformationJobObject(self.handle, 3, listing, size, None):
                    count = ctypes.c_ulong.from_buffer(listing, 4).value
                    ids = (ctypes.c_size_t * count).from_buffer(listing, 8)
                    for pid in ids:
                        handle = self.api.OpenProcess(0x100000, False, pid)  # SYNCHRONIZE
                        if handle:
                            members.append(handle)
                    break
                if ctypes.get_last_error() != 234 or size >= 1048576:
                    raise ctypes.WinError(ctypes.get_last_error())
                size *= 2
            try:
                if not self.api.TerminateJobObject(self.handle, 124):
                    raise ctypes.WinError(ctypes.get_last_error())
                for handle in members:
                    if self.api.WaitForSingleObject(handle, 5000) != 0:
                        return False
            finally:
                for handle in members:
                    self.api.CloseHandle(handle)
            deadline = time.monotonic() + 5
            while True:
                info = ACCOUNTING()
                if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
                    raise ctypes.WinError(ctypes.get_last_error())
                if not info.ActiveProcesses:
                    return True
                if time.monotonic() > deadline:
                    return False
                time.sleep(.02)

        def close(self):
            if self.handle:
                self.api.CloseHandle(self.handle)
                self.handle = None


def run_process(command, cwd, env, timeout, stdout, stderr):
    """Return raw process evidence. A timeout always cleans the whole process tree."""
    if timeout <= 0:
        raise ProcessTimeout("Suite deadline expired")
    started = time.monotonic()
    job = WindowsJob() if os.name == "nt" else None
    proc = None
    timed_out = False
    cleanup_ok = False
    try:
        proc = subprocess.Popen(
            [str(a) for a in command], cwd=cwd, env=env,
            stdout=stdout, stderr=stderr, stdin=subprocess.DEVNULL,
            creationflags=(subprocess.CREATE_NO_WINDOW | 0x4) if job else 0,
            start_new_session=not bool(job))
        if job:
            job.attach_and_resume(proc)
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
    finally:
        if job:
            try:
                cleanup_ok = job.cleanup()
            finally:
                job.close()
        elif proc:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            cleanup_ok = True
        if proc:
            proc.wait(timeout=5)
    return {"command": [str(a) for a in command], "exitCode": proc.returncode,
            "workingDirectory": str(cwd),
            "timedOut": timed_out, "cleanupVerified": cleanup_ok,
            "durationSeconds": round(time.monotonic() - started, 6)}


@contextlib.contextmanager
def resource_locks(directory: Path, resources, timeout):
    """OS locks are released on crashes; lock files themselves are not ownership."""
    directory.mkdir(parents=True, exist_ok=True)
    handles = []
    deadline = time.monotonic() + timeout
    try:
        for resource in sorted(set(resources)):
            handle = (directory / (resource + ".lock")).open("a+b")
            handles.append(handle)
            handle.seek(0, 2)
            if not handle.tell():
                handle.write(b"0")
                handle.flush()
            while True:
                try:
                    handle.seek(0)
                    if os.name == "nt":
                        import msvcrt
                        msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                    else:
                        import fcntl
                        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise ProcessTimeout("Resource lock timed out: " + resource)
                    time.sleep(.05)
        yield
    finally:
        for handle in reversed(handles):
            # Closing the descriptor releases the advisory/exclusive lock.
            handle.close()
