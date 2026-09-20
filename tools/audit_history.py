#!/usr/bin/env python3
"""Heuristic full-history text scan. Run from a full clone; never prints secrets.

This is a screening aid, not certification. Binary content, external LFS objects,
GitHub attachments and unreachable objects require separate review.
"""
import argparse
import json
import re
import subprocess
from pathlib import Path

RULES = {
    'private_key': r'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----',
    'github_token': r'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,})\b',
    'aws_access_key': r'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b',
    'slack_token': r'\bxox[baprs]-[A-Za-z0-9-]{20,}',
    'bearer_literal': r'(?i)bearer\s+[A-Za-z0-9_.-]{24,}',
    'credential_assignment': r'''(?i)(?:api[_-]?key|password|secret|access[_-]?token)\s*[:=]\s*["'][^"'\n]{12,}["']''',
    'personal_path': r'''/(?:Users|home)/[^/\s"']+|[A-Z]:\\Users\\[^\\\s]+''',
    'workspace_path': r'''/workspace/[^\s"']+''',
    'email': r'[A-Za-z0-9_.+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path('.'))
    args = parser.parse_args()

    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(args.repo), *arguments])

    if git('rev-parse', '--is-shallow-repository').strip() != b'false':
        raise SystemExit('Use a full clone, not a shallow checkout.')
    findings, binaries, scanned = [], [], 0
    objects = git('rev-list', '--objects', '--all').decode().splitlines()
    for entry in objects:
        sha, _, path = entry.partition(' ')
        if git('cat-file', '-t', sha).strip() != b'blob':
            continue
        data = git('cat-file', 'blob', sha)
        try:
            if b'\0' in data:
                raise UnicodeError()
            text = data.decode('utf-8')
        except UnicodeError:
            binaries.append({'sha': sha, 'path': path, 'bytes': len(data)})
            continue
        scanned += 1
        for name, pattern in RULES.items():
            count = len(re.findall(pattern, text))
            if count:
                findings.append({'rule': name, 'sha': sha, 'path': path, 'count': count})
    print(json.dumps({'head': git('rev-parse', 'HEAD').decode().strip(),
                      'text_blobs_scanned': scanned, 'findings': findings,
                      'binaries_requiring_review': binaries,
                      'limitations': 'Pattern screening only; review matches and binaries manually.'}, indent=2))
    return 1 if findings else 0


if __name__ == '__main__':
    raise SystemExit(main())
