#!/usr/bin/env python3
"""Seeded CSV/Python/Swift differential tests plus CLI integration checks."""
from pathlib import Path
import csv, html, importlib.util, io, random, subprocess, sys, tempfile
from html.parser import HTMLParser
root = Path(__file__).resolve().parents[1]
subprocess.run(['swift', 'build', '--product', 'yacht'], cwd=root, check=True)
binary = Path(subprocess.check_output(['swift', 'build', '--show-bin-path'], cwd=root, text=True).strip())/'yacht'
spec=importlib.util.spec_from_file_location('legacy_yacht',root/'legacy-python/yacht.py')
legacy=importlib.util.module_from_spec(spec);sys.modules['legacy_yacht']=legacy;spec.loader.exec_module(legacy)
class Cells(HTMLParser):
    def __init__(self): super().__init__(); self.cells=[]; self.inside=False; self.tags=[]
    def handle_starttag(self,tag,attrs):
        self.tags.append(tag)
        if tag in ('td','th'): self.inside=True; self.cells.append('')
    def handle_endtag(self,tag):
        if tag in ('td','th'): self.inside=False
    def handle_data(self,data):
        if self.inside: self.cells[-1]+=data
rng=random.Random(20260915)
values=['', 'a,b', '"quoted"', "O'Reilly", '<script>alert(1)</script>', 'a&b', 'line\nnext', 'CR\rLF\r\n', '🛥️', '日本語', 'e\u0301', '17%', '12345', '-1,234.50', ' whitespace ', '"\u0301', '<\u0301', '&\ufe0f']
with tempfile.TemporaryDirectory(prefix='yacht-regression-') as tmp:
    base=Path(tmp)
    for i in range(60):
        width=rng.randint(1,8)
        records=[[rng.choice(values) for _ in range(width)] for _ in range(rng.randint(2,40))]
        # csv.writer with CRLF quotes both CR and LF, regardless of host.
        stream=io.StringIO(newline='');csv.writer(stream,lineterminator='\r\n').writerows(records)
        source=base/f'case-{i}.csv';source.write_bytes((('\ufeff' if i%2 else '')+stream.getvalue()).encode())
        output=source.with_suffix('.html')
        subprocess.run([str(binary),str(source)],check=True,capture_output=True)
        parsed=Cells();parsed.feed(output.read_bytes().decode())
        assert parsed.cells == [cell for row in records for cell in row], i
        assert 'script' not in parsed.tags and 'img' not in parsed.tags, i
        expected=Cells();expected.feed(legacy.render_csv_to_html_doc(source,legacy.StyleOptions()))
        assert parsed.cells==expected.cells, i
    source=base/'case-0.csv'; output=source.with_suffix('.html'); previous=output.read_bytes()
    assert subprocess.run([str(binary),str(source)],capture_output=True).returncode != 0
    assert output.read_bytes()==previous
    subprocess.run([str(binary),str(source),'--overwrite','--font-size','22','--zebra','no','--border-style','dashed','--hover=false'],check=True,capture_output=True)
    assert 'font-size: 22px' in output.read_text() and 'nth-child' not in output.read_text()
    invalid=base/'bad.csv';invalid.write_text('A\n"broken')
    good=base/'good.csv';good.write_text('A\nvalid')
    assert subprocess.run([str(binary),str(invalid),str(good)],capture_output=True).returncode != 0
    assert good.with_suffix('.html').exists() and not invalid.with_suffix('.html').exists()
    assert subprocess.run([str(binary),str(source),'--output',str(source),'--overwrite'],capture_output=True).returncode != 0
    assert subprocess.run([str(binary),str(source),'--font-family','</style><script>'],capture_output=True).returncode != 0
print('60 seeded Python/Swift CSV regressions and CLI safety/batch checks passed.')
