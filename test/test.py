from tqdm import tqdm
import json
import subprocess
import random

with open('/Users/jack/.bash_history') as history_file:
    lines = history_file.readlines()
    lines = [line.rstrip() for line in lines]
    tokens = {token: True for line in lines for token in line.split()}

tokens = list(tokens.keys())
for i in tqdm(range(100000)):
    cmd = [random.choice(tokens).replace('"', '').replace('\'', '') for i in range(5)]
    subprocess.run(['../build/histx', 'index'] + cmd)
