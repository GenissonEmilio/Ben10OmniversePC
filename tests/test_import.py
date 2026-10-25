"""Authored synthetic fixtures; original game data is never needed in CI."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from import_game import import_iso
from ps3_metadata import parse_sfo


def make_sfo(title_id="BLUS31017", version="01.00"):
    pairs={"TITLE":"BEN 10 OMNIVERSE™","TITLE_ID":title_id,"APP_VER":version,"CATEGORY":"DG"}
    names,values,entries=bytearray(),bytearray(),bytearray()
    for key,value in pairs.items():
        raw=value.encode()+b"\0"
        entries.extend(struct.pack("<HHIII",len(names),0x204,len(raw),len(raw),len(values)))
        names.extend(key.encode()+b"\0");values.extend(raw)
    start=20+len(entries)
    return b"\0PSF"+struct.pack("<4I",0x101,start,start+len(names),len(pairs))+entries+names+values


@unittest.skipUnless(shutil.which("genisoimage"), "genisoimage required for fixtures")
class Import(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.base=Path(self.temp.name)
        game=self.base/"disc/PS3_GAME"
        (game/"USRDIR").mkdir(parents=True)
        self.sfo=game/"PARAM.SFO";self.sfo.write_bytes(make_sfo())
        self.binary=b"SCE\0"+bytes(124)
        (game/"USRDIR/EBOOT.BIN").write_bytes(self.binary)

    def tearDown(self):
        self.temp.cleanup()

    def iso(self, joliet=False):
        iso=self.base/"own-fixture.iso"
        subprocess.run(["genisoimage","-quiet","-iso-level","3",*(["-J"] if joliet else []),"-o",str(iso),str(self.base/"disc")],check=True)
        return iso

    def test_import_is_atomic_and_does_not_claim_gameplay(self):
        iso=self.iso();before=hashlib.sha256(iso.read_bytes()).hexdigest()
        with patch("import_game.EBOOT_SHA256",hashlib.sha256(self.binary).hexdigest()):
            report=import_iso(iso,self.base/"installed",lambda _:None)
        self.assertEqual(report["title_id"],"BLUS31017")
        self.assertFalse(report["native_gameplay_validated"])
        self.assertEqual((self.base/"installed/PS3_GAME/USRDIR/EBOOT.BIN").read_bytes(),self.binary)
        self.assertEqual(hashlib.sha256(iso.read_bytes()).hexdigest(),before)

    def test_rejects_another_game_before_creating_destination(self):
        self.sfo.write_bytes(make_sfo("BCES00510"))
        with self.assertRaisesRegex(ValueError,"BLUS31017"):
            import_iso(self.iso(),self.base/"installed",lambda _:None)
        self.assertFalse((self.base/"installed").exists())

    def test_rejects_modified_executable(self):
        with self.assertRaisesRegex(ValueError,"Executável diferente"):
            import_iso(self.iso(),self.base/"installed",lambda _:None)
        self.assertFalse((self.base/"installed").exists())

    def test_existing_saves_and_destination_are_preserved(self):
        dest=self.base/"installed";dest.mkdir();(dest/"owner-save").write_bytes(b"saved")
        with self.assertRaisesRegex(ValueError,"preservados"):
            import_iso(self.iso(),dest,lambda _:None)
        self.assertEqual((dest/"owner-save").read_bytes(),b"saved")

    def test_joliet_preserves_names_opened_by_the_game_on_linux(self):
        folder=self.base/"disc/PS3_GAME/USRDIR/Configurations"
        folder.mkdir()
        (folder/"0000000100000001.cfg").write_bytes(b"config")
        with patch("import_game.EBOOT_SHA256",hashlib.sha256(self.binary).hexdigest()):
            import_iso(self.iso(joliet=True),self.base/"installed",lambda _:None)
        self.assertEqual((self.base/"installed/PS3_GAME/USRDIR/Configurations/0000000100000001.cfg").read_bytes(),b"config")


class Metadata(unittest.TestCase):
    def test_truncated_tables_and_values_are_rejected(self):
        valid=make_sfo()
        for length in (0,19,20,len(valid)-1):
            with self.subTest(length=length),self.assertRaises(ValueError):
                parse_sfo(valid[:length])


if __name__=="__main__":
    unittest.main()
