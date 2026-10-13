import re,shlex,subprocess,sys,struct
from pathlib import Path
build=Path(sys.argv[1]).resolve()
commands=Path(sys.argv[2]).read_text()
assert not re.search(r'(?:-I|-L|-isystem\s*)(?:/opt/homebrew|/usr/local)/(?:include|lib)',commands),'Host include/library path leaked'
line=next(line for line in commands.splitlines() if '-o WiiCompiledGraphicsTests.app/WiiCompiledGraphicsTests' in line)
tokens=shlex.split(line)
archives=[build/token for token in tokens if token.endswith('.a')]
assert archives,'No static libraries found in app link command'
def ios_object(data,name):
    magic,cpu,subtype,kind,ncmds,sz,flags,reserved=struct.unpack_from('<8I',data)
    assert magic==0xfeedfacf and cpu==0x0100000c and kind==1,name
    offset=32; found=False
    for _ in range(ncmds):
        cmd,size=struct.unpack_from('<2I',data,offset)
        if cmd==0x32:
            assert struct.unpack_from('<I',data,offset+8)[0]==2,name
            found=True
        offset+=size
    assert found,name
count=0
for archive in archives:
    data=archive.read_bytes(); assert data.startswith(b'!<arch>\n'),archive
    offset=8; objects=0
    while offset+60<=len(data):
        header=data[offset:offset+60]; size=int(header[48:58]); name=header[:16].decode().strip()
        body=data[offset+60:offset+60+size]
        if name.startswith('#1/'):
            name_size=int(name[3:]); name=body[:name_size].decode().rstrip('\0'); body=body[name_size:]
        if body[:4]==b'\xcf\xfa\xed\xfe':
            ios_object(body,str(archive)+':'+name); objects+=1
        elif not name.startswith(('__.SYMDEF','/')):
            raise AssertionError(f'Unknown archive member {archive}:{name}')
        offset+=60+size+(size%2)
    assert objects,archive
    print(f'{archive.name}: {objects} arm64 physical iOS objects PASS'); count+=objects
print(f'Linked archives: {len(archives)}, physical iOS objects: {count}, host header/library exclusion: PASS')
