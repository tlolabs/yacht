from pathlib import Path

from setuptools import setup

APP = ["yacht.py"]
OPTIONS = {
    "argv_emulation": True,
    "plist": {
        "CFBundleName": "Y.A.C.H.T.",
        "CFBundleDisplayName": "Y.A.C.H.T.",
        "CFBundleIdentifier": "com.local.yacht.csvhtmltranslator",
        "CFBundleVersion": "1.1.0",
        "CFBundleShortVersionString": "1.1.0",
        "CFBundleDocumentTypes": [
            {
                "CFBundleTypeName": "CSV File",
                "CFBundleTypeExtensions": ["csv"],
                "CFBundleTypeRole": "Editor",
            }
        ],
    },
}

setup(
    name="yacht",
    version="1.1.0",
    description="Y.A.C.H.T. - Yet Another CSV HTML Translator",
    long_description=Path("README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    url="https://github.com/tlolabs/yacht",
    license="GPL-3.0-only",
    app=APP,
    data_files=["BTE.csv", "LICENSE", "README.md"],
    options={"py2app": OPTIONS},
    setup_requires=["py2app"],
)
