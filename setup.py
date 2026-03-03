from setuptools import setup

APP = ["yacht.py"]
OPTIONS = {
    "argv_emulation": True,
    "plist": {
        "CFBundleName": "Y.A.C.H.T.",
        "CFBundleDisplayName": "Y.A.C.H.T.",
        "CFBundleIdentifier": "com.local.yacht.csvhtmltranslator",
        "CFBundleVersion": "1.0.0",
        "CFBundleShortVersionString": "1.0.0",
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
    app=APP,
    options={"py2app": OPTIONS},
    setup_requires=["py2app"],
)
