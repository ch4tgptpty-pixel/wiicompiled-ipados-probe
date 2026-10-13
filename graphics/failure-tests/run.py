import subprocess,sys
from pathlib import Path
result=0
for case in ('error','loss','fence','map','ui'):
    path=Path('artifacts/logs')/(case+'.txt')
    try:
        proc=subprocess.run([sys.argv[1],case],capture_output=True,text=True,timeout=2 if case=='map' else 10)
        path.write_text(proc.stdout+proc.stderr+f'\nExit code: {proc.returncode}\n')
        if proc.returncode: result=1
    except subprocess.TimeoutExpired:
        path.write_text('FAIL: callback blocked beyond test deadline\n'); result=1
    print(case+': '+path.read_text())
sys.exit(result)
