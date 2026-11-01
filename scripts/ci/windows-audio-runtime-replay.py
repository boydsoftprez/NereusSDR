#!/usr/bin/env python3
# no-port-check: NereusSDR-original diagnostic artifact/PE reader.
# 2026-10-31 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
# Static PE import/export decoding follows Microsoft's PE/COFF format:
# https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
# No application DLL is loaded/executed by this script. No registry writes.
# The resolution graph is a documented normal-search model, not a claim
# that a debugger observed the original failed runner's loaded modules.
import struct,json,hashlib,pathlib,sys
class PE:
 def __init__(self,p):
  self.p=p;self.d=p.read_bytes();self.u16=lambda o:struct.unpack_from('<H',self.d,o)[0];self.u32=lambda o:struct.unpack_from('<I',self.d,o)[0];self.u64=lambda o:struct.unpack_from('<Q',self.d,o)[0]
  self.h=self.u32(0x3c);assert self.d[self.h:self.h+4]==b'PE\0\0';self.opt=self.h+24;self.magic=self.u16(self.opt);self.bits=64 if self.magic==0x20b else 32;self.dirs=self.opt+(112 if self.bits==64 else 96)
  start=self.opt+self.u16(self.h+20);self.sections=[]
  for q in range(start,start+40*self.u16(self.h+6),40):
   self.sections.append((self.u32(q+12),max(self.u32(q+8),self.u32(q+16)),self.u32(q+20),self.u32(q+16)))
 def off(self,r):
  for va,n,raw,rn in self.sections:
   if va<=r<va+n:
    if r-va>=rn:raise ValueError('Unbacked RVA '+hex(r))
    return raw+r-va
  if r<self.u32(self.opt+60):return r
  raise ValueError('Unmapped RVA '+hex(r))
 def z(self,r):
  o=self.off(r);end=self.d.index(0,o);return self.d[o:end].decode('ascii',errors='replace')
 def imports(self):
  rva=self.u32(self.dirs+8);out={}
  if not rva:return out
  q=self.off(rva)
  while self.u32(q+12):
   dll=self.z(self.u32(q+12));t=self.off(self.u32(q) or self.u32(q+16));names=[]
   while True:
    v=self.u64(t) if self.bits==64 else self.u32(t);t+=self.bits//8
    if not v:break
    names.append('#'+str(v&0xffff) if v&(1<<(self.bits-1)) else self.z(v+2))
   out[dll]=names;q+=20
  return out
 def exports(self):
  rva,size=self.u32(self.dirs),self.u32(self.dirs+4);out={}
  if not rva:return out
  q=self.off(rva);base=self.u32(q+16);nf=self.u32(q+20);nn=self.u32(q+24);ft=self.off(self.u32(q+28));nt=self.off(self.u32(q+32)) if nn else 0;ot=self.off(self.u32(q+36)) if nn else 0
  for i in range(nf):
   addr=self.u32(ft+4*i)
   if addr:out['#'+str(base+i)]={'rva':hex(addr),'forwarder':self.z(addr) if rva<=addr<rva+size else None}
  for i in range(nn):
   name=self.z(self.u32(nt+4*i));out[name]=out['#'+str(base+self.u16(ot+2*i))]
  return out
 def summary(self):return {'path':str(self.p),'size':len(self.d),'sha256':hashlib.sha256(self.d).hexdigest(),'machine':hex(self.u16(self.h+4)),'timestamp':hex(self.u32(self.h+8)),'bits':self.bits,'linker_version':str(self.d[self.opt+2])+'.'+str(self.d[self.opt+3]),'image_size':self.u32(self.opt+56),'imports':self.imports(),'exports':self.exports()}

EXPECTED_FILES = {'third_party/fftw3/bin/libfftw3-3.dll': {'sha256': '001a835d2f25ac0661580d67db5622157c28481c06a7f3ef45ad939839bf54a1', 'size': 2712765}, 'third_party/fftw3/bin/libfftw3f-3.dll': {'sha256': '42ca18fff35dd12890e04478bc990005b3969cb744f6843976bd436ccd7f0a4c', 'size': 2772692}, 'build/windows-audio-tests.xml': {'sha256': '97e942493cc74b8ab9fa1f40249f49bf12489a946ba4fd687bcdf7c0b56039e8', 'size': 1417}, 'build/windows-audio-imports.txt': {'sha256': 'e36b355510eb0d9f54665a925fce2817ba92fdf04ce87cfabb16f5593b5ff6d2', 'size': 1238}, 'build/libNereusCore.dll': {'sha256': '4fab90090f83c3948402765cb59e055dc55d34c67c94130928e2a31aa4122e62', 'size': 138380696}, 'build/libNereusSDRLib.dll': {'sha256': 'be4c9da9e7cc9187829f7910bc43caa79f0a312ce79cae99e365f0438fd13c65', 'size': 106547874}, 'build/librade.dll': {'sha256': '0ae6ae9ccfa786f260afa60aa069f6d35d6b95072d2c5e1aae44bd473c89aba7', 'size': 21750017}, 'build/CMakeCache.txt': {'sha256': '90eea1ea45424d8afff42bde3fe90c6629c04a47acaa42417fd6190db0e915b4', 'size': 82228}, 'build/windows-audio-selected.txt': {'sha256': '1725bb566703a1a0e82e2f1b18fe032691a2b528e6310cb3cbe8d5aaada9b714', 'size': 2046}, 'build/tests/tst_wasapi_policy.exe': {'sha256': 'bf600b086d66b1f8526905d5c177ea2467fa1782690f014a6ba01e0635649390', 'size': 137133}, 'build/tests/tst_wasapi_notifier_lifetime.exe': {'sha256': 'f22eb380104d65687fdea2900cfecba23ef60543e8dd15ec779bbfc6a94cf8a1', 'size': 372326}, 'build/tests/tst_audio_device_catalog.exe': {'sha256': '5c1eacdce0160e39c074d4696b90b037e8c8eebb5a562dde08ee2a4e33e5b621', 'size': 1030548}, 'build/Testing/Temporary/LastTestsFailed.log': {'sha256': 'c4f39f6d9f05683f3361765c463839728881c51fedcf52f4a766953edc99057d', 'size': 128}, 'build/Testing/Temporary/CTestCostData.txt': {'sha256': 'b368b282675720453c9c586886e14bcfaf67c9cbd3ef7d550e8a7175856a0937', 'size': 245}, 'build/Testing/Temporary/LastTest.log': {'sha256': '7ba7d057bc67bf29044f9b5d9acf5b86331ce698ce19faf376de6f2b9337d34a', 'size': 3019}}

import argparse
import os
import re
import shutil
from collections import deque

API_SET_PREFIXES = ('api-ms-', 'ext-ms-')
ROOT_TESTS = ('tst_wasapi_policy', 'tst_wasapi_notifier_lifetime', 'tst_audio_device_catalog')

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

def sha(path):
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
        return digest.hexdigest()

def verify(root, out):
    checked = []
    for relative, expected in EXPECTED_FILES.items():
        path = root / relative
        actual = {'path': relative, 'size': path.stat().st_size, 'sha256': sha(path)}
        if actual['size'] != expected['size'] or actual['sha256'] != expected['sha256']:
            raise RuntimeError('Artifact mismatch: ' + relative)
        checked.append(actual)
    write_json(out / 'artifact-verified.json', {
        'run': 41258145196, 'artifact_id': 13064574188,
        'artifact_zip_sha256': 'e8030ab7dbe54817866966de58040a51086b002b0c9a5f58d614ce2c00ff8737',
        'files': checked})
    # The diagnostic artifact preserves executables, not generated CTest files.
    # Recreate ONLY the four already-reviewed registrations, no configure/build.
    lines = ['# Reconstructed from the preserved ac9 CTest listing; no build.\n']
    for name, binary, group in (
        ('tst_wasapi_policy', 'tst_wasapi_policy', None),
        ('tst_wasapi_notifier_lifetime', 'tst_wasapi_notifier_lifetime', 'rest'),
        ('tst_wasapi_notifier_lifetime_native', 'tst_wasapi_notifier_lifetime', 'native'),
        ('tst_audio_device_catalog', 'tst_audio_device_catalog', None)):
        exe = (root / 'build/tests' / (binary + '.exe')).as_posix()
        build = (root / 'build').as_posix()
        cwd = (root / 'build/tests').as_posix()
        labels = 'core'
        if binary == 'tst_wasapi_notifier_lifetime':
            labels += ';windows-com-lifetime'
        if group == 'native':
            labels += ';windows-com-native'
        if binary == 'tst_audio_device_catalog':
            labels += ';realtime'
        environment = 'NEREUS_WORKSPACE_PREVIEW=0'
        if group:
            environment += ';NEREUS_WASAPI_NOTIFIER_TEST_GROUP=' + group
        lines.append(f'add_test({name} "{exe}")\n')
        lines.append(f'set_tests_properties({name} PROPERTIES TIMEOUT 120 WORKING_DIRECTORY "{cwd}" '
                     f'LABELS "{labels}" ENVIRONMENT "{environment}" ENVIRONMENT_MODIFICATION '
                     f'"PATH=path_list_prepend:{build};QT_QPA_PLATFORM=set:offscreen")\n')
    (root / 'build/CTestTestfile.cmake').write_text(''.join(lines), encoding='utf-8')
    print('Verified all 15 preserved artifact members and wrote four no-build CTest registrations.')

def audit(root, out):
    if os.name != 'nt':
        raise RuntimeError('Provider search audit requires the isolated Windows runner, not an emulated local PATH.')
    import winreg
    system_root = pathlib.Path(os.environ['SystemRoot']).resolve()
    system32 = system_root / 'System32'
    known = {}
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                        r'SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs',
                        0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
        index = 0
        while True:
            try:
                name, value, kind = winreg.EnumValue(key, index)
            except OSError:
                break
            index += 1
            if isinstance(value, str) and value.lower().endswith('.dll'):
                known[value.lower()] = str(system32 / value)
    # Ac9 CTest prepended build to the Test step's PATH; child working
    # directory was build/tests. Environment below is the declared ac9
    # action setup reproduced now, not a recovered original full PATH.
    effective_path = str(root / 'build') + os.pathsep + os.environ.get('PATH', '')
    test_dir = root / 'build/tests'
    search = [test_dir, system32, system_root, test_dir]
    search += [pathlib.Path(p.strip('"')) for p in effective_path.split(os.pathsep) if p]
    search = list(dict.fromkeys(str(p.resolve()) for p in search if p.is_dir()))
    write_json(out / 'environment.json', {
        'original_runner_full_path_preserved': False,
        'reproduced_declared_ac9_setup_path': os.environ.get('PATH', ''),
        'effective_test_path': effective_path, 'working_directory': str(test_dir),
        'modeled_normal_search_directories': search, 'known_dlls': known,
        'qt_root': os.environ.get('QT_ROOT_DIR'),
        'openssl_root': os.environ.get('OPENSSL_ROOT_DIR'),
        'limitations': ['Normal unpackaged DLL search modeled; no claim of observed loaded-module selection.',
                        'API-set contracts require the Windows schema and are not guessed as filesystem DLLs.',
                        'SxS/loaded-module redirection is not a static filesystem observation.']})
    records, candidates, edges, failures, platform_unknown = {}, {}, [], [], []
    artifact_paths = {str((root / name).resolve()).lower() for name in EXPECTED_FILES}
    seen_checks, loaded = set(), {}

    def is_system(path):
        return str(path.resolve()).lower().startswith(str(system_root).lower() + os.sep)

    def describe(path):
        path = path.resolve()
        key = str(path).lower()
        if key not in records:
            if len(records) >= 256:
                raise RuntimeError('Audit exceeded its 256-image bound; see retained partial evidence.')
            digest = sha(path)
            retained = None
            if not is_system(path) and key not in artifact_paths:
                destination = out / 'providers' / digest / path.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                if not destination.exists():
                    shutil.copyfile(path, destination)
                retained = str(destination)
            # Preserve a provider's bytes before parsing; even an invalid
            # image must remain available when the parser reports failure.
            try:
                record = PE(path).summary()
            except Exception as error:
                records[key] = {'path': str(path), 'sha256': digest,
                                'system_provider': is_system(path),
                                'retained_copy': retained, 'parse_error': str(error)}
                raise
            record['system_provider'] = is_system(path)
            if retained:
                record['retained_copy'] = retained
            records[key] = record
        return records[key]

    def choose(dll):
        lower = dll.lower()
        if lower.startswith(API_SET_PREFIXES):
            return None, 'api-set'
        if lower in loaded:
            return loaded[lower], 'previously-selected-in-this-static-graph'
        choices = []
        if lower in known and pathlib.Path(known[lower]).is_file():
            choices.append(pathlib.Path(known[lower]))
        for directory in search:
            path = pathlib.Path(directory) / dll
            if path.is_file() and str(path.resolve()).lower() not in {str(p.resolve()).lower() for p in choices}:
                choices.append(path)
        candidates[lower] = [str(p.resolve()) for p in choices]
        if not choices:
            return None, 'not-found-in-recorded-normal-search'
        loaded[lower] = choices[0]
        return choices[0], 'known-dll' if lower in known else 'first-normal-search-candidate'

    def check(consumer, dll, symbol, chain=()):
        identity = (str(consumer).lower(), dll.lower(), symbol)
        if identity in seen_checks:
            return
        seen_checks.add(identity)
        if len(chain) > 16:
            failures.append({'kind': 'forwarder-depth-bound', 'consumer': str(consumer), 'dll': dll, 'symbol': symbol})
            return
        provider, selection = choose(dll)
        edge = {'consumer': str(consumer), 'dll': dll, 'symbol': symbol,
                'selection': selection, 'forwarder_chain': list(chain)}
        if provider is None:
            if selection == 'api-set':
                platform_unknown.append(edge)
            else:
                failures.append(dict(edge, kind='dll-not-found'))
            return
        record = describe(provider)
        edge.update(provider=str(provider.resolve()), provider_sha256=record['sha256'])
        exported = record['exports'].get(symbol)
        if exported is None:
            failure = dict(edge, kind='entrypoint-not-exported')
            failure['candidate_comparison'] = []
            for candidate in candidates.get(dll.lower(), [str(provider)]):
                candidate_record = describe(pathlib.Path(candidate))
                failure['candidate_comparison'].append({
                    'path': candidate, 'sha256': candidate_record['sha256'],
                    'exports_requested_symbol': symbol in candidate_record['exports']})
            failures.append(failure)
            print('MISSING ENTRYPOINT:', json.dumps(failure), flush=True)
        else:
            edges.append(edge)
            if exported['forwarder']:
                module, name = exported['forwarder'].rsplit('.', 1)
                if not module.lower().endswith('.dll'):
                    module += '.dll'
                check(provider, module, name, chain + (str(provider) + '!' + symbol,))

    try:
        # Preserve all direct external providers/candidates (the original
        # artifact has fourteen non-system names) before graph traversal.
        # A later malformed provider must not discard the other evidence.
        artifact_images = [root / name for name in EXPECTED_FILES
                           if pathlib.Path(name).suffix.lower() in ('.exe', '.dll')]
        artifact_names = {image.name.lower() for image in artifact_images}
        direct_external = set()
        for image in artifact_images:
            direct_external.update(dll for dll in describe(image)['imports']
                                   if dll.lower() not in artifact_names)
        for dll in sorted(direct_external):
            provider, selection = choose(dll)
            for candidate in candidates.get(dll.lower(), []):
                if not is_system(pathlib.Path(candidate)):
                    try:
                        describe(pathlib.Path(candidate))
                    except Exception as error:
                        failures.append({'kind': 'provider-parse-error', 'dll': dll,
                                         'provider': candidate, 'error': str(error)})
        for relative in EXPECTED_FILES:
            if pathlib.Path(relative).suffix.lower() not in ('.exe', '.dll'):
                continue
            # Cover all eight preserved images (including the GUI DLL),
            # under the test search layout, without executing any of them.
            # Each executable's process module namespace is independent.
            loaded.clear()
            pending = deque([root / relative])
            traversed = set()
            while pending:
                consumer = pending.popleft().resolve()
                if str(consumer).lower() in traversed:
                    continue
                traversed.add(str(consumer).lower())
                record = describe(consumer)
                if record['machine'] != '0x8664':
                    failures.append({'kind': 'machine-mismatch', 'path': str(consumer), 'machine': record['machine']})
                # Inspect only requested system exports/forwarders, not all
                # system imports. Keep OS/API-set scope explicitly bounded.
                if record['system_provider']:
                    continue
                for dll, imports in record['imports'].items():
                    provider, selection = choose(dll)
                    for symbol in imports:
                        check(consumer, dll, symbol)
                    if provider is not None and not is_system(provider):
                        pending.append(provider)
    finally:
        write_json(out / 'pe-runtime-audit.json', {
            'records': list(records.values()), 'normal_search_candidates': candidates,
            'matched_imports': edges, 'failures': failures,
            'platform_resolution_not_guessed': platform_unknown,
            'limitations': ['Provider selection is a recorded normal-search model, not original-runner runtime tracing.',
                            'System imports are not recursively scanned; requested exports/forwarders are checked.',
                            'Delay-load/plugin imports are outside this startup import-table audit.']})
        print('AUDIT:', len(records), 'images;', len(edges), 'matched imports;',
              len(failures), 'failures;', len(platform_unknown), 'explicit API-set unknowns.', flush=True)
    if failures:
        return 2
    return 0

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Verify preserved ac9 tests or statically audit their Windows runtime providers; never loads them.')
    parser.add_argument('mode', choices=('verify', 'audit'))
    parser.add_argument('--root', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    try:
        if args.mode == 'verify':
            verify(root, out)
            status = 0
        else:
            status = audit(root, out)
    except Exception as error:
        write_json(out / (args.mode + '-error.json'), {'error': type(error).__name__, 'message': str(error)})
        print(type(error).__name__ + ': ' + str(error), file=sys.stderr)
        status = 3
    raise SystemExit(status)
