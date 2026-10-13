import sys, plistlib, struct, zipfile
from pathlib import Path
ipa=Path(sys.argv[1])
with zipfile.ZipFile(ipa) as z:
    assert z.testzip() is None
    prefix='Payload/WiiCompiledGraphicsTests.app/'
    info=plistlib.loads(z.read(prefix+'Info.plist'))
    assert info['CFBundleIdentifier']=='org.local.wiicompiled.graphicstests'
    assert info['UIDeviceFamily']==[2]
    assert info['MinimumOSVersion']=='16.0'
    binary=z.read(prefix+info['CFBundleExecutable'])
    magic,cpu,subtype,kind,ncmds,sz,flags,reserved=struct.unpack_from('<8I',binary)
    assert magic==0xfeedfacf and cpu==0x0100000c and kind==2
    offset=32
    platforms=[]
    for _ in range(ncmds):
        cmd,size=struct.unpack_from('<2I',binary,offset)
        assert size>=8 and offset+size<=len(binary)
        if cmd==0x32:
            platform,minimum,sdk=struct.unpack_from('<3I',binary,offset+8)
            assert platform==2 and minimum==0x00100000
            platforms.append(platform)
        if cmd in (0xc,0x80000018,0x8000001f):
            start=struct.unpack_from('<I',binary,offset+8)[0]
            library=binary[offset+start:offset+size].split(b'\0',1)[0].decode()
            assert library.startswith(('/usr/lib/','/System/Library/')),library
        offset+=size
    assert platforms==[2]
    assert not any(n.lower().endswith(('.rvz','.dol','.rel','.szs','.brstm')) for n in z.namelist())
print('IPA ZIP, bundle, arm64 physical iOS16, system-only dylibs, no game resources: PASS')
