#!/usr/bin/env python3
"""Seeded CSV round-trip regressions plus CLI integration checks."""
from pathlib import Path
import csv, io, random, subprocess, sys, tempfile
from html.parser import HTMLParser
root = Path(__file__).resolve().parents[1]
subprocess.run(['cargo', 'build', '--locked', '-p', 'yacht-cli'], cwd=root, check=True)
binary = root/'target'/'debug'/('yacht.exe' if sys.platform == 'win32' else 'yacht')
class Cells(HTMLParser):
    def __init__(self): super().__init__(); self.cells=[]; self.inside=False; self.tags=[]
    def handle_starttag(self,tag,attrs):
        self.tags.append(tag)
        if tag in ('td','th'): self.inside=True; self.cells.append('')
    def handle_endtag(self,tag):
        if tag in ('td','th'): self.inside=False
    def handle_data(self,data):
        if self.inside: self.cells[-1]+=data
rng=random.Random(20260916)
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
    source=base/'case-0.csv'; output=source.with_suffix('.html'); previous=output.read_bytes()
    assert subprocess.run([str(binary),str(source)],capture_output=True).returncode != 0
    assert output.read_bytes()==previous
    subprocess.run([str(binary),str(source),'--overwrite','--font-size','22','--zebra','no','--border-style','dashed','--hover=false'],check=True,capture_output=True)
    assert 'font-size: 22px' in output.read_text(encoding='utf-8') and 'nth-child' not in output.read_text(encoding='utf-8')
    # Every retained style flag reaches the one shared generator.
    all_flags=['--table-class','teaching','--font-family',"'Times New Roman', serif",'--font-size','20','--cell-padding','12','--border-width','3','--border-style','dotted','--border-color','rebeccapurple','--header-bg','rgb(1, 2, 3)','--header-text-color','white','--body-bg','#ffeecc','--zebra','true','--zebra-bg','#abcdef','--hover','true','--hover-bg','#fedcba','--border-collapse','separate','--border-spacing','4']
    subprocess.run([str(binary),str(source),'--overwrite',*all_flags],check=True,capture_output=True)
    complete=output.read_text(encoding='utf-8')
    for token in ['csv-table teaching',"'Times New Roman', serif",'20px','12px','3px dotted rebeccapurple','rgb(1, 2, 3)','color: white','#ffeecc','#abcdef','#fedcba','border-collapse: separate','border-spacing: 4px']:
        assert token in complete, token
    subprocess.run([str(binary),str(source),'--overwrite','--font-size','23','--unstyled'],check=True,capture_output=True)
    assert '<style>' not in output.read_text(encoding='utf-8')
    positional=base/'-input.csv';positional.write_text('A\n1',encoding='utf-8')
    subprocess.run([str(binary),'--','-input.csv'],cwd=base,check=True,capture_output=True)
    assert positional.with_suffix('.html').exists()
    invalid=base/'bad.csv';invalid.write_text('A\n"broken',encoding='utf-8')
    good=base/'good.csv';good.write_text('A\nvalid',encoding='utf-8')
    assert subprocess.run([str(binary),str(invalid),str(good)],capture_output=True).returncode != 0
    assert good.with_suffix('.html').exists() and not invalid.with_suffix('.html').exists()
    assert subprocess.run([str(binary),str(source),'--output',str(source),'--overwrite'],capture_output=True).returncode != 0
    assert subprocess.run([str(binary),str(source),'--font-family','</style><script>'],capture_output=True).returncode != 0
print('60 seeded CSV round-trip regressions and CLI safety/batch checks passed.')
