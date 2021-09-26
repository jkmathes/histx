import subprocess
from tqdm import tqdm

with open('/Users/jack/.bash_history') as history_file:
    lines = history_file.readlines()
    lines = [line.rstrip() for line in lines]

for line in tqdm(lines):
    torun = ['bash', '-c', '../build/histx index ' + line]
    subprocess.run(torun)
