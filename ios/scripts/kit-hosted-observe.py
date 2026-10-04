#!/usr/bin/env python3
# NereusSDR: isolated hosted Kit observation; no retries or workload changes
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import threading
import time

BASE = 'd2ca3475e30556494a70bd78a0c72eee32aa9a9c'
ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'ios/NereusKit'


def stamp():
    return {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'monotonic': time.monotonic()}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def names_helper_product(args, helper, products):
    fields = args.split()
    flag = '--test-bundle-path'
    if not fields or fields[0] != helper or fields.count(flag) != 1 or any(f.startswith('--list-tests') for f in fields):
        return None
    index = fields.index(flag)
    if index + 1 >= len(fields) or fields[index + 1] not in products:
        return None
    if fields.count('--testing-library') != 1:
        return None
    library = fields.index('--testing-library')
    if library + 1 >= len(fields) or fields[library + 1] != 'swift-testing':
        return None
    product = fields[index + 1]
    if not product.endswith('/NereusKitPackageTests.xctest/Contents/MacOS/NereusKitPackageTests'):
        return None
    return product


def helper_phase(args, helper, products):
    fields = args.split()
    discovery = fields.count('--list-tests')
    if discovery > 1 or any(f.startswith('--list-tests') and f != '--list-tests' for f in fields):
        return None
    stripped = ' '.join(f for f in fields if f != '--list-tests')
    product = names_helper_product(stripped, helper, products)
    if product is None:
        return None
    return ('discovery' if discovery else 'execution', product)


def same_identity(expected, current):
    return (expected is not None and current is not None
            and all(expected[k] == current[k] for k in ('pid', 'lstart', 'image', 'pgid')))


def owned_descendants(snapshot, wrapper_pid):
    owned = {wrapper_pid}
    while True:
        extra = {pid for pid, row in snapshot.items() if row['ppid'] in owned}
        if extra <= owned:
            return owned
        owned |= extra


def snapshot_processes():
    text = subprocess.check_output(['ps', '-axo', 'pid=,ppid=,pgid=,lstart=,comm='], text=True)
    result = {}
    for line in text.splitlines():
        fields = line.strip().split(None, 8)
        if len(fields) == 9:
            pid, ppid, pgid = map(int, fields[:3])
            result[pid] = {'pid': pid, 'ppid': ppid, 'pgid': pgid,
                           'lstart': ' '.join(fields[3:8]), 'image': fields[8]}
    return result


def same_observation(a, b):
    return same_identity(a, b) and a['ppid'] == b['ppid']


def current_chain(snapshot, wrapper, pid):
    if wrapper is None or not same_observation(wrapper, snapshot.get(wrapper['pid'])):
        return None
    chain, visited = [], set()
    while pid not in visited:
        row = snapshot.get(pid)
        if row is None:
            return None
        chain.append(row)
        if pid == wrapper['pid']:
            return chain
        visited.add(pid)
        pid = row['ppid']
    return None


def consistent_chain(before, after, wrapper, pid):
    first, last = current_chain(before, wrapper, pid), current_chain(after, wrapper, pid)
    if (first is None or last is None or len(first) != len(last)
            or not all(same_observation(a, b) for a, b in zip(first, last))):
        return None
    return last


def admit_owned(before, after, wrapper, pid, args, helper, available):
    chain = consistent_chain(before, after, wrapper, pid)
    if chain is None or not args or args.split()[0] != chain[0]['image']:
        return None
    runner = chain[0]['image'] == helper
    if runner and helper_phase(args, helper, available) is None:
        return None
    if (not runner and ('swiftpm-testing-helper' in chain[0]['image']
                        or '.xctest/Contents/MacOS/' in chain[0]['image'])):
        return None
    return {'identity': chain[0], 'args': args, 'chain': chain, 'runner': runner}


def cleanup_disposition(record, before, after, wrapper, args, helper, available):
    expected = record['identity']
    first, last = before.get(expected['pid']), after.get(expected['pid'])
    if not same_identity(expected, first) or not same_observation(first, last) or args != record['args']:
        return None
    if record['runner'] and helper_phase(args, helper, available) is None:
        return None
    chain = consistent_chain(before, after, wrapper, expected['pid'])
    if chain is not None:
        # A different live chain never inherits the original admission.
        if [r['pid'] for r in chain] != [r['pid'] for r in record['chain']]:
            return None
        if not all(same_identity(a, b) for a, b in zip(chain, record['chain'])):
            return None
        return 'current verified chain'
    # Only a previously admitted lifetime can become an explicit orphan.
    # An unknown PID or a live replacement parent is never admitted here.
    if expected['pid'] != wrapper['pid'] and last['ppid'] == 1 and len(record['chain']) > 1:
        parent = record['chain'][1]
        if parent['pid'] not in before and parent['pid'] not in after:
            return 'previously verified orphan; original parent absent'
    return None


def read_args(pid):
    try:
        return subprocess.check_output(['ps', '-p', str(pid), '-o', 'args='], text=True).strip()
    except subprocess.CalledProcessError:
        return ''


def observe_owned(wrapper, pid, helper, available, observation=None):
    before = snapshot_processes()
    if current_chain(before, wrapper, pid) is None:
        return None
    args = read_args(pid)
    after = snapshot_processes()
    if observation is not None:
        observation.update(before=before.get(pid), after=after.get(pid), args=args)
    return admit_owned(before, after, wrapper, pid, args, helper, available)


def token_overlap(tail, chunk, token=b'Test run started.'):
    data = tail + chunk
    return data.count(token), data[-(len(token) - 1):]


def service_sampler(child, metadata, receipt, deadline, final=False,
                    clock=time.monotonic, get_identity=None, get_args=None):
    get_identity = identity if get_identity is None else get_identity
    get_args = read_args if get_args is None else get_args
    if 'exit' in metadata or 'failed_guard' in metadata:
        return
    remaining = deadline - clock()
    if final and child.poll() is None and remaining > 0:
        try:
            child.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            pass
    code = child.poll()
    if code is not None and clock() <= deadline:
        metadata['exit'] = code
        metadata['observed_finished'] = stamp()
        if code != 0:
            metadata['failed_guard'] = 'sampler returned nonzero'
            receipt['failed_guard'] = metadata['failed_guard']
        return
    if clock() < deadline:
        return
    metadata['failed_guard'] = 'sampler processing exceeded original 10-second deadline'
    receipt['failed_guard'] = metadata['failed_guard']
    for action in ('terminate', 'kill'):
        current = get_identity(child.pid)
        args = get_args(child.pid) if same_identity(metadata['sampler_identity'], current) else ''
        last = get_identity(child.pid)
        if (not same_observation(current, last) or not same_identity(metadata['sampler_identity'], current)
                or current['ppid'] != os.getpid() or args != metadata['sampler_args']):
            metadata['cleanup_rejected'] = current
            return
        if child.poll() is not None:
            metadata['exit'] = child.poll()
            return
        getattr(child, action)()
        try:
            metadata['exit'] = child.wait(timeout=2)
            return
        except subprocess.TimeoutExpired:
            pass
    metadata['survived_cleanup'] = True

def identity(pid):
    try:
        text = subprocess.check_output(['ps', '-p', str(pid), '-o', 'lstart=', '-o', 'ppid=',
                                        '-o', 'pgid=', '-o', 'comm='], text=True)
    except subprocess.CalledProcessError:
        return None
    fields = text.strip().split(None, 7)
    if len(fields) != 8:
        return None
    return {'pid': pid, 'lstart': ' '.join(fields[:5]), 'ppid': int(fields[5]),
            'pgid': int(fields[6]), 'image': fields[7]}


def source_manifest():
    return {str(p.relative_to(ROOT)): digest(p) for p in SOURCE.rglob('*')
            if p.is_file() and '.build' not in p.parts and '.swiftpm' not in p.parts}


def production_matches_base(manifest):
    names = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', BASE,
                                    'ios/NereusKit/Sources', 'ios/NereusKit/Package.swift',
                                    'ios/scripts/swift-test.sh'], cwd=ROOT, text=True).splitlines()
    mismatches = []
    for name in names:
        blob = subprocess.check_output(['git', 'show', BASE + ':' + name], cwd=ROOT)
        actual = manifest.get(name) or digest(ROOT / name)
        if hashlib.sha256(blob).hexdigest() != actual:
            mismatches.append(name)
    # Additional source files would change the product too.
    expected_sources = {n for n in names if '/Sources/' in n}
    actual_sources = {n for n in manifest if '/Sources/' in n}
    mismatches.extend(sorted(actual_sources - expected_sources))
    return mismatches


def products():
    build = SOURCE / '.build'
    return {str(p.resolve()): digest(p) for p in build.rglob('*') if p.is_file()
            and p.parent.name == 'MacOS' and p.parent.parent.name == 'Contents'
            and p.parent.parent.parent.suffix == '.xctest'}


def selected_helper():
    swift = Path(subprocess.check_output(['xcrun', '--find', 'swift'], text=True).strip())
    helper = swift.parent.parent / 'libexec/swift/pm/swiftpm-testing-helper'
    if not helper.is_file():
        raise RuntimeError('selected toolchain has no recognized testing helper')
    return str(helper)


def write_json(out, name, value):
    (out / name).write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def cleanup_owned(proc, known, receipt, helper, available):
    wrapper = receipt['wrapper']
    snapshot = snapshot_processes()
    owned = owned_descendants(snapshot, proc.pid)
    receipt['cleanup_unknown'] = [snapshot[p] for p in owned if p in snapshot and p not in known]
    receipt.setdefault('cleanup_actions', [])
    receipt.setdefault('cleanup_rejected', [])
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for pid, record in sorted(known.items(), reverse=True):
            before = snapshot_processes()
            if not same_identity(record['identity'], before.get(pid)):
                continue
            args = read_args(pid)
            after = snapshot_processes()
            disposition = cleanup_disposition(record, before, after, wrapper, args, helper, available)
            if disposition is None:
                receipt['cleanup_rejected'].append({'record': record, 'current': after.get(pid), 'args': args})
                continue
            try:
                os.kill(pid, sig)
                receipt['cleanup_actions'].append({'identity': record['identity'], 'signal': int(sig),
                                                    'disposition': disposition})
            except ProcessLookupError:
                pass
        if sig == signal.SIGTERM:
            end = time.monotonic() + 5
            while time.monotonic() < end and any(same_identity(v['identity'], identity(p)) for p, v in known.items()):
                time.sleep(0.1)
    receipt['cleanup_survivors'] = [v for p, v in known.items() if same_identity(v['identity'], identity(p))]
    current = snapshot_processes()
    receipt['unknown_survivors'] = [row for row in receipt['cleanup_unknown'] if same_identity(row, current.get(row['pid']))]

def receipt_format_valid(exported):
    controls = []
    for item in exported:
        entries = item['entries']
        if (item.get('record_capacity') != 512 or item.get('name_utf8_capacity') != 128
                or item.get('phase_utf8_capacity') != 256 or len(entries) > 512
                or len(item['name'].encode('utf-8')) > 128):
            return False
        if any(len(e['phase'].encode('utf-8')) > 256 or e['uptime'] > e['append_uptime']
               or e['sequence'] != index for index, e in enumerate(entries)):
            return False
        if item['name'] == 'captured append order control':
            controls.append(entries)
    if len(controls) != 1 or len(controls[0]) != 2:
        return False
    after, captured = controls[0]
    return (after['phase'] == 'after existing awaits'
            and captured['phase'] == 'captured before existing awaits'
            and captured['uptime'] < after['uptime']
            and captured['append_uptime'] >= after['append_uptime'])


def main():
    out = Path(tempfile.mkdtemp(prefix='kit-hosted-observe-', dir=os.environ.get('RUNNER_TEMP')))
    receipts_dir = out / 'receipts'
    receipts_dir.mkdir()
    if os.environ.get('GITHUB_ENV'):
        with open(os.environ['GITHUB_ENV'], 'a') as env:
            env.write('KIT_HOSTED_ARTIFACT_DIR=' + str(out) + '\n')
    print('Kit diagnostic artifact directory: ' + str(out), flush=True)
    before = source_manifest()
    write_json(out, 'source-before.json', before)
    unchanged = production_matches_base(before)
    receipt = {'start': stamp(), 'argv': ['ios/scripts/swift-test.sh'], 'source_count': len(before),
               'base': BASE, 'production_mismatches': unchanged, 'samples': [], 'owned_processes': [],
               'processor_count': os.cpu_count(), 'load_before': os.getloadavg(),
               'python_monotonic_clock_info': vars(time.get_clock_info('monotonic'))}
    for label, argv in [('swift', ['swift', '--version']), ('xcode', ['xcodebuild', '-version']),
                        ('developer', ['xcode-select', '-p']), ('os', ['sw_vers']),
                        ('head', ['git', 'rev-parse', 'HEAD'])]:
        receipt[label] = subprocess.check_output(argv, cwd=ROOT, text=True).strip()
    write_json(out, 'receipt.json', receipt)
    if unchanged:
        raise RuntimeError('production/vendor/package/wrapper differs from frozen base')
    helper = selected_helper()
    if not receipt['swift'].startswith('Apple Swift version 6.3.3 '):
        raise RuntimeError('hosted compiler differs from the requested Swift 6.3.3 observation')
    receipt['selected_helper'] = helper
    environment = dict(os.environ, KIT_HOSTED_RECEIPTS_DIR=str(receipts_dir))
    proc = subprocess.Popen(receipt['argv'], cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, start_new_session=True)
    receipt['wrapper'] = identity(proc.pid)
    initial = observe_owned(receipt['wrapper'], proc.pid, helper, {})
    if initial is None:
        receipt['unknown_wrapper_survivor'] = identity(proc.pid)
        receipt['failed_guard'] = 'wrapper lifetime/ancestry could not be admitted'
        write_json(out, 'receipt.json', receipt)
        raise RuntimeError('unknown wrapper; no signal authorized')
    known = {proc.pid: initial}
    output = {'run_starts': 0}
    output_lock = threading.Lock()

    def reader():
        pending = b''
        with (out / 'raw.log').open('wb') as log:
            while True:
                chunk = proc.stdout.read1(65536)
                if not chunk:
                    break
                log.write(chunk)
                log.flush()
                count, pending = token_overlap(pending, chunk)
                with output_lock:
                    output['run_starts'] += count
                # Console emission is explicitly not an occurrence timestamp.
                print(chunk.decode('utf-8', errors='replace'), end='', flush=True)

    reading = threading.Thread(target=reader, daemon=True)
    reading.start()
    began = time.monotonic()
    test = None
    test_start = None
    samples = []
    sample_numbers = set()
    failure = None
    try:
        while proc.poll() is None:
            now = time.monotonic()
            if now - began > 1200:
                failure = 'wrapper/compile exceeded 20-minute failed backstop'
                break
            snapshot = snapshot_processes()
            owned = owned_descendants(snapshot, proc.pid)
            for pid in sorted(owned):
                if pid in snapshot and pid not in known:
                    candidate = observe_owned(receipt['wrapper'], pid, helper, products() if snapshot[pid]['image'] == helper else {})
                    if candidate is not None:
                        known[pid] = candidate
                        receipt['owned_processes'].append({'record': candidate, 'observed': stamp()})
            helpers = [snapshot[p] for p in owned if p in snapshot and snapshot[p]['image'] == helper]
            unknown = [snapshot[p] for p in owned if p in snapshot
                       and ('swiftpm-testing-helper' in snapshot[p]['image'] or '.xctest/Contents/MacOS/' in snapshot[p]['image'])
                       and snapshot[p]['image'] != helper]
            if unknown or len(helpers) > 1:
                receipt['unknown_graph'] = unknown + helpers
                receipt['unknown_owned_arguments'] = []
                for row in unknown + helpers:
                    first = snapshot_processes()
                    if current_chain(first, receipt['wrapper'], row['pid']) is not None:
                        args = read_args(row['pid'])
                        last = snapshot_processes()
                        if consistent_chain(first, last, receipt['wrapper'], row['pid']) is not None:
                            receipt['unknown_owned_arguments'].append({'identity': last[row['pid']], 'args': args})
                failure = 'unknown or multiple test runners'
                break
            if helpers and test is None:
                candidate = helpers[0]
                available = products()
                attempt = {}
                observed = observe_owned(receipt['wrapper'], candidate['pid'], helper, available, attempt)
                if observed is None:
                    # A disappearing discovery helper is not a replacement lifetime.
                    if identity(candidate['pid']) is None:
                        continue
                    receipt['unrecognized_runner'] = {'candidate': candidate, 'products': available, 'observation': attempt}
                    failure = 'runner lifetime/current ancestry/argv not recognized'
                    break
                first = last = observed['identity']
                args = observed['args']
                phase = helper_phase(args, helper, available)
                if len(available) != 1 or phase is None:
                    receipt['unrecognized_runner'] = {'before': first, 'after': last, 'args': args, 'products': available}
                    failure = 'runner identity/argv does not name one exact aggregate product'
                    break
                if candidate['pid'] in known and not same_identity(known[candidate['pid']]['identity'], last):
                    failure = 'runner PID replaced an admitted lifetime'
                    break
                known[candidate['pid']] = observed
                if phase[0] == 'discovery':
                    # Discovery is not test execution, even when it uses the
                    # same executable and --testing-library token.
                    receipt.setdefault('discovery_helpers', {})[str(first['pid'])] = {'identity': first, 'args': args}
                    time.sleep(0.1)
                    continue
                product = phase[1]
                if not same_identity(first, last) or len(available) != 1 or product is None:
                    receipt['unrecognized_runner'] = {'before': first, 'after': last, 'args': args, 'products': available}
                    failure = 'runner identity/argv does not name one exact aggregate product'
                    break
                test = {'identity': last, 'args': args, 'product': product}
                test_start = time.monotonic()
                receipt['test'] = test
                receipt['test_verified'] = stamp()
                receipt['products_before_test_measurement'] = available
                write_json(out, 'products-before-test.json', available)
                print('Kit aggregate test identity verified', flush=True)
            with output_lock:
                starts = output['run_starts']
            if starts > 1:
                failure = 'more than one aggregate test run'
                break
            if test is not None:
                age = now - test_start
                if age > 120:
                    failure = 'verified test exceeded 120-second failed backstop'
                    break
                for number, threshold in [(1, 10), (2, 20)]:
                    if age < threshold or number in sample_numbers:
                        continue
                    observed = observe_owned(receipt['wrapper'], test['identity']['pid'], helper, {test['product']})
                    if observed is None:
                        if identity(test['identity']['pid']) is None:
                            continue
                        failure = 'test current ownership/argv changed before sample'
                        break
                    current, args = observed['identity'], observed['args']
                    if (not same_identity(test['identity'], current)
                            or names_helper_product(args, helper, {test['product']}) != test['product']):
                        failure = 'test arguments/identity changed before sample'
                        break
                    sample_numbers.add(number)
                    argv = ['/usr/bin/sample', str(current['pid']), '1', '10', '-mayDie', '-fullPaths',
                            '-file', str(out / ('sample-' + str(number) + '.txt'))]
                    handle = (out / ('sample-' + str(number) + '-command.log')).open('wb')
                    sample_deadline = time.monotonic() + 10
                    child = subprocess.Popen(argv, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
                    metadata = {'number': number, 'argv': argv, 'start': stamp(), 'verified_test': current,
                                'verified_args': args, 'sampler_identity': identity(child.pid),
                                'deadline_monotonic': sample_deadline}
                    metadata['sampler_args'] = read_args(child.pid)
                    admitted_sampler = identity(child.pid)
                    if (not same_observation(metadata['sampler_identity'], admitted_sampler)
                            or admitted_sampler['ppid'] != os.getpid()
                            or metadata['sampler_args'].split() != argv):
                        metadata['failed_guard'] = 'sampler lifetime/argv could not be admitted; no signal authorized'
                        receipt['failed_guard'] = metadata['failed_guard']
                        failure = metadata['failed_guard']
                    samples.append((child, handle, metadata, sample_deadline))
            for child, _, metadata, deadline in samples:
                service_sampler(child, metadata, receipt, deadline)
                if metadata.get('failed_guard'):
                    failure = metadata['failed_guard']
            if failure:
                break
            if now - began > 1200:
                failure = 'wrapper/compile exceeded 20-minute failed backstop'
                break
            # A logged run without a recognized helper may never inherit compile budget.
            if starts and test is None:
                failure = 'test started without a verified aggregate helper'
                break
            time.sleep(0.1)
    except Exception as error:
        failure = 'observation exception: ' + repr(error)
    finally:
        if failure:
            receipt['failed_guard'] = failure
            cleanup_owned(proc, known, receipt, helper, products())
        try:
            receipt['exit'] = proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            receipt['failed_guard'] = 'wrapper survived verified cleanup'
        reading.join(timeout=5)
        if reading.is_alive():
            receipt['failed_guard'] = 'output reader did not close'
        for child, handle, metadata, deadline in samples:
            service_sampler(child, metadata, receipt, deadline, final=True)
            if child.poll() is None:
                metadata['unknown_or_surviving_sampler'] = identity(child.pid)
                receipt['failed_guard'] = 'sampler survived or could not be verified for cleanup'
            handle.close()
            receipt['samples'].append(metadata)
        surviving = [v for p, v in known.items() if same_identity(v['identity'], identity(p))]
        if surviving:
            receipt['failed_guard'] = 'owned descendants survived wrapper termination'
            cleanup_owned(proc, known, receipt, helper, products())
        after = source_manifest()
        write_json(out, 'source-after.json', after)
        receipt['sources_equal'] = before == after
        receipt['products_after'] = products()
        receipt['end'] = stamp()
        receipt['load_after'] = os.getloadavg()
        raw = (out / 'raw.log').read_text(errors='replace')
        receipt['raw_sha256'] = digest(out / 'raw.log')
        receipt['run_starts'] = raw.count('Test run started.')
        receipt['terminal'] = [line for line in raw.splitlines() if 'Test run with ' in line]
        receipt['issues'] = [line for line in raw.splitlines() if 'recorded an issue' in line]
        receipt['skips'] = [line for line in raw.splitlines() if ' skipped:' in line or ' skipped.' in line]
        exported = [json.loads(p.read_text()) for p in receipts_dir.glob('*.json')]
        receipt['receipt_files'] = len(exported)
        receipt['receipt_format_valid'] = receipt_format_valid(exported)
        receipt['dropped_records'] = sum(item['dropped'] for item in exported)
        receipt['diagnostic_sources_after_match_base'] = production_matches_base(after)
        receipt['export_failure'] = 'KIT HOSTED DIAGNOSTIC EXPORT FAILED' in raw
        receipt['artifact_hashes'] = {str(p.relative_to(out)): digest(p) for p in out.rglob('*') if p.is_file() and p.name != 'receipt.json'}
        write_json(out, 'receipt.json', receipt)
    diagnostic_ok = (not receipt.get('failed_guard') and test is not None and receipt['sources_equal']
                     and receipt['products_before_test_measurement'] == receipt['products_after']
                     and not receipt['diagnostic_sources_after_match_base']
                     and all(s.get('exit') == 0 and not s.get('failed_guard') for s in receipt['samples'])
                     and receipt['run_starts'] == 1 and len(receipt['terminal']) == 1
                     and receipt['receipt_format_valid'] and receipt['receipt_files'] > 0 and receipt['dropped_records'] == 0 and not receipt['export_failure'])
    if not diagnostic_ok:
        raise RuntimeError('diagnostic observation incomplete; see retained receipt')
    return receipt['exit']


if __name__ == '__main__':
    raise SystemExit(main())
