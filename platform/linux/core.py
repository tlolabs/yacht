"""ctypes ownership adapter for the versioned Rust ABI (no application logic)."""

import ctypes
import json
import os
from pathlib import Path


class CoreError(Exception):
    def __init__(self, error):
        super().__init__(error["message"])
        self.code = error["code"]


class Core:
    def __init__(self, library=None):
        library = (
            library
            or os.environ.get("YACHT_LIBRARY")
            or Path(__file__).with_name("libyacht_ffi.so")
        )
        self.library = ctypes.CDLL(str(library))
        self.callback_type = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p)
        self.library.yacht_request.argtypes = [
            ctypes.c_char_p,
            self.callback_type,
            ctypes.c_void_p,
        ]
        self.library.yacht_request.restype = ctypes.c_void_p
        self.library.yacht_free.argtypes = [ctypes.c_void_p]
        self.library.yacht_free.restype = None

    def call(self, operation, cancel=None, **args):
        callback = self.callback_type(lambda _: bool(cancel and cancel.is_set()))
        request = json.dumps(
            dict(version=1, op=operation, **args), ensure_ascii=False
        ).encode("utf-8")
        pointer = self.library.yacht_request(request, callback, None)
        if not pointer:
            raise RuntimeError("The Rust core returned no response.")
        try:
            response = json.loads(ctypes.string_at(pointer).decode("utf-8"))
        finally:
            self.library.yacht_free(pointer)
        if "error" in response:
            raise CoreError(response["error"])
        return response["ok"]

    def table(self, op="sample", **args):
        return Table(self, self.call(op, **args))


class Table:
    def __init__(self, core, metadata):
        self.core, self.metadata = core, metadata

    def call(self, op, **args):
        return self.core.call(op, handle=self.metadata["handle"], **args)

    def __del__(self):
        try:
            self.core.call("release", handle=self.metadata["handle"])
        except Exception:
            pass  # interpreter shutdown only
